#include <cmath>

#include <AnalyzerChannelData.h>
#include <AnalyzerHelpers.h>

#include "JoybusAnalyzer.h"
#include "JoybusAnalyzerSettings.h"

// How long the line has to stay high before the bus counts as idle, used to resync
// after a run the decoder could not make sense of
#define JOYBUS_BUS_IDLE_NS     100000

// Symbol low time as a per-mille fraction of the bit time, upper bound for each symbol.
// The nominal low times are 250, 500 and 750, so each threshold sits midway between two symbols.
#define SYMBOL_ONE_MAX         375
#define SYMBOL_TARGET_STOP_MAX 625
#define SYMBOL_ZERO_MAX        900

// Bit time bounds. Official devices run between 202.5 kHz and 262.144 kHz, which is
// 3815 ns to 4938 ns per bit. The window is widened to tolerate sampling error.
#define BIT_TIME_MIN_NS        3000
#define BIT_TIME_MAX_NS        6000

// A transmitter has let go of the line once nothing pulls it low again within this many
// bit times. The slowest measured reply, from a GameCube to Game Boy Advance cable, comes
// back about 1.6 bit times after the stop bit that asked for it.
#define SYMBOL_TERMINAL_BITS   3

// Ceiling on the symbols in one run, to bound the buffer when the line is noisy.
// The longest official transfer is a 35 byte write followed by a one byte reply.
#define MAX_RUN_SYMBOLS        2050

JoybusAnalyzer::JoybusAnalyzer() : Analyzer2(), mSettings(), mSimulationInitilized( false )
{
    SetAnalyzerSettings( &mSettings );
    UseFrameV2();
}

JoybusAnalyzer::~JoybusAnalyzer()
{
    KillThread();
}

void JoybusAnalyzer::SetupResults()
{
    mResults.reset( new JoybusAnalyzerResults( this, &mSettings ) );
    SetAnalyzerResults( mResults.get() );
    mResults->AddChannelBubblesWillAppearOn( mSettings.mDataChannel );
}

U64 JoybusAnalyzer::SamplesToNs( U64 samples )
{
    return ( samples * 1000000000 ) / mSampleRateHz;
}

void JoybusAnalyzer::AdvanceToBusIdle()
{
    if( mJoybus->GetBitState() == BIT_LOW )
        mJoybus->AdvanceToNextEdge();

    while( SamplesToNs( mJoybus->GetSampleOfNextEdge() - mJoybus->GetSampleNumber() ) < JOYBUS_BUS_IDLE_NS )
    {
        mJoybus->AdvanceToNextEdge();
        mJoybus->AdvanceToNextEdge();
    }

    mCurrentPhase = JOYBUS_PHASE_IDLE;
}

JoybusSymbolKind JoybusAnalyzer::ClassifySymbol( const JoybusSymbol& symbol, double bit_time )
{
    // Every symbol begins with a falling edge and differs only in how long the line is
    // held low, measured against the bit time the transmitter is running at
    double low_permille = ( symbol.rise - symbol.fall ) * 1000.0 / bit_time;

    if( low_permille < SYMBOL_ONE_MAX )
        return JOYBUS_SYMBOL_ONE;

    if( low_permille < SYMBOL_TARGET_STOP_MAX )
        return JOYBUS_SYMBOL_TARGET_STOP;

    if( low_permille < SYMBOL_ZERO_MAX )
        return JOYBUS_SYMBOL_ZERO;

    return JOYBUS_SYMBOL_INVALID;
}

void JoybusAnalyzer::FitGrid( const std::vector<JoybusSymbol>& symbols, size_t first, size_t last, double& period, double& error )
{
    // One transmitter clocks every falling edge in a transmission, one bit time apart,
    // so they fall on a straight line against their own index. Every edge is fitted
    // rather than just the two ends, which keeps one noisy edge from tilting the line.
    size_t count = last - first + 1;
    double mean_index = ( count - 1 ) / 2.0;
    double mean_fall = 0.0;

    for( size_t index = first; index <= last; index++ )
        mean_fall += ( double )symbols[ index ].fall;

    mean_fall /= count;

    double covariance = 0.0;
    double variance = 0.0;

    for( size_t index = first; index <= last; index++ )
    {
        double offset = ( double )( index - first ) - mean_index;
        covariance += offset * ( ( double )symbols[ index ].fall - mean_fall );
        variance += offset * offset;
    }

    // Spans are never shorter than a byte, so the indices always spread
    period = covariance / variance;

    // Root mean square distance from that line, which averages jitter down over the
    // span while a transmission boundary inside it stays as a bend
    double total = 0.0;

    for( size_t index = first; index <= last; index++ )
    {
        double residual = ( double )symbols[ index ].fall - ( mean_fall + period * ( ( double )( index - first ) - mean_index ) );
        total += residual * residual;
    }

    error = sqrt( total / count );
}

bool JoybusAnalyzer::GetSymbolRun( std::vector<JoybusSymbol>& symbols )
{
    symbols.clear();

    U64 bit_time = 0;

    for( ;; )
    {
        JoybusSymbol symbol;

        // We're sitting on the symbol's falling edge
        symbol.fall = mJoybus->GetSampleNumber();

        // Move to the rising edge that ends the low period
        mJoybus->AdvanceToNextEdge();
        symbol.rise = mJoybus->GetSampleNumber();

        if( bit_time == 0 )
        {
            // A transmission is at least one byte plus a stop bit, so its first symbol
            // is always a data bit and its falling edge spacing is one bit time
            symbol.terminal = false;
            symbol.next_fall = mJoybus->GetSampleOfNextEdge();

            bit_time = symbol.next_fall - symbol.fall;

            U64 bit_time_ns = SamplesToNs( bit_time );
            if( bit_time_ns < BIT_TIME_MIN_NS || bit_time_ns > BIT_TIME_MAX_NS )
                return false;
        }
        else
        {
            // Once a transmitter has let go of the line nothing pulls it low again for
            // far longer than the next bit was due
            U64 terminal_at = symbol.fall + ( bit_time * SYMBOL_TERMINAL_BITS );
            symbol.terminal = !mJoybus->WouldAdvancingToAbsPositionCauseTransition( terminal_at );
            symbol.next_fall = symbol.terminal ? 0 : mJoybus->GetSampleOfNextEdge();
        }

        symbols.push_back( symbol );

        if( symbol.terminal )
            return true;

        if( symbols.size() > MAX_RUN_SYMBOLS )
            return false;

        // Move to the falling edge of the next symbol
        mJoybus->AdvanceToNextEdge();
    }
}

size_t JoybusAnalyzer::FindStopBit( const std::vector<JoybusSymbol>& symbols )
{
    size_t count = symbols.size();

    // Every transmission is a whole number of bytes followed by one stop bit, so a run
    // holding a single transmission ends on its stop bit
    if( count % 8 == 1 )
        return count - 1;

    // Anything else has to be a command the target replied to before the line had been
    // high long enough to end the run, which needs a byte aligned split
    if( count % 8 != 2 )
        return 0;

    size_t best = 0;
    double best_error = 0.0;

    // A reply needs a byte and a stop bit of its own, so the split stops short of that
    for( size_t index = 8; index + 9 < count; index += 8 )
    {
        // Split the run in two and fit each half to its own bit time. Only the real
        // stop bit leaves the host's edges on one line and the target's on another;
        // anywhere else one half straddles the handover and bends.
        double command_period, command_error;
        double reply_period, reply_error;
        FitGrid( symbols, 0, index, command_period, command_error );
        FitGrid( symbols, index + 1, count - 1, reply_period, reply_error );

        // Three quarter bits low is a data zero, which is never a stop bit. A host stop
        // bit is the same shape as a data one, so shape alone gets no further.
        JoybusSymbolKind kind = ClassifySymbol( symbols[ index ], command_period );
        if( kind == JOYBUS_SYMBOL_ZERO || kind == JOYBUS_SYMBOL_INVALID )
            continue;

        double error = ( reply_error > command_error ) ? reply_error : command_error;

        if( best == 0 || error < best_error )
        {
            best = index;
            best_error = error;
        }
    }

    return best;
}

void JoybusAnalyzer::AddByteFrame( U8 byte, U64 start, U64 end )
{
    // Add a bubble for the byte
    Frame frame;
    frame.mStartingSampleInclusive = start;
    frame.mEndingSampleInclusive = end;
    frame.mType = mCurrentPhase;
    frame.mData1 = byte;
    mResults->AddFrame( frame );

    // Add a frame for the byte
    FrameV2 framev2;
    framev2.AddByte( "data", byte );
    framev2.AddString( "type", GetPhaseName() );
    mResults->AddFrameV2( framev2, "byte", start, end );

    mResults->CommitResults();
}

void JoybusAnalyzer::AddStopBitFrame( const JoybusSymbol& symbol, double bit_time )
{
    // Add a stop marker in the middle of low period
    U64 bit_mid = ( symbol.fall + symbol.rise ) / 2;
    mResults->AddMarker( bit_mid, AnalyzerResults::Stop, mSettings.mDataChannel );

    // Add a frame for the stop bit
    FrameV2 framev2;
    framev2.AddString( "type", ClassifySymbol( symbol, bit_time ) == JOYBUS_SYMBOL_TARGET_STOP ? "target" : "host" );
    mResults->AddFrameV2( framev2, "stop", symbol.fall, symbol.rise - 1 );

    mResults->CommitResults();
}

void JoybusAnalyzer::EmitTransmission( const std::vector<JoybusSymbol>& symbols, size_t first, size_t stop )
{
    // One clock drives the whole transmission, so fitting its falling edges gives a bit
    // time steadier than any single pair of them
    double bit_time, error;
    FitGrid( symbols, first, stop, bit_time, error );

    // The stop bit says which end was driving the line. A host holds it low for one
    // quarter bit and a target for two.
    bool from_target = ClassifySymbol( symbols[ stop ], bit_time ) == JOYBUS_SYMBOL_TARGET_STOP;
    mCurrentPhase = from_target ? JOYBUS_PHASE_RESPONSE : JOYBUS_PHASE_COMMAND;

    U8 byte = 0;
    U8 bit_index = 0;
    U64 byte_start = symbols[ first ].fall;

    for( size_t index = first; index < stop; index++ )
    {
        const JoybusSymbol& symbol = symbols[ index ];
        bool is_one = ClassifySymbol( symbol, bit_time ) == JOYBUS_SYMBOL_ONE;

        // Add a zero/one marker in the middle of the bit
        U64 bit_mid = ( symbol.fall + symbol.next_fall ) / 2;
        mResults->AddMarker( bit_mid, is_one ? AnalyzerResults::One : AnalyzerResults::Zero, mSettings.mDataChannel );

        // Bits arrive most significant first
        byte = ( byte << 1 ) | ( is_one ? 1 : 0 );

        if( ++bit_index == 8 )
        {
            AddByteFrame( byte, byte_start, symbol.next_fall - 1 );

            // The first byte of a command is the opcode and the rest are its arguments
            if( mCurrentPhase == JOYBUS_PHASE_COMMAND )
                mCurrentPhase = JOYBUS_PHASE_ARGS;

            byte = 0;
            bit_index = 0;
            byte_start = symbol.next_fall;
        }
    }

    AddStopBitFrame( symbols[ stop ], bit_time );
}

void JoybusAnalyzer::GetTransaction()
{
    // Start at falling edge
    mJoybus->AdvanceToNextEdge();

    std::vector<JoybusSymbol> symbols;
    if( !GetSymbolRun( symbols ) )
    {
        AdvanceToBusIdle();
        return;
    }

    size_t stop = FindStopBit( symbols );
    if( stop == 0 )
    {
        AdvanceToBusIdle();
        return;
    }

    EmitTransmission( symbols, 0, stop );

    // A target that replied fast shares the run with the command it replied to
    if( stop + 1 < symbols.size() )
        EmitTransmission( symbols, stop + 1, symbols.size() - 1 );
}

const char* JoybusAnalyzer::GetPhaseName()
{
    switch( mCurrentPhase )
    {
    case JOYBUS_PHASE_COMMAND:
        return "command";
    case JOYBUS_PHASE_ARGS:
        return "argument";
    case JOYBUS_PHASE_RESPONSE:
        return "response";
    case JOYBUS_PHASE_IDLE:
    default:
        return "idle";
    }
}

void JoybusAnalyzer::WorkerThread()
{
    mSampleRateHz = GetSampleRate();
    mJoybus = GetAnalyzerChannelData( mSettings.mDataChannel );

    // Wait for bus idle before reading transactions
    AdvanceToBusIdle();

    // Read Joybus transactions forever
    for( ;; )
    {
        GetTransaction();

        ReportProgress( mJoybus->GetSampleNumber() );
        CheckIfThreadShouldExit();
    }
}

bool JoybusAnalyzer::NeedsRerun()
{
    return false;
}

U32 JoybusAnalyzer::GenerateSimulationData( U64 minimum_sample_index, U32 device_sample_rate,
                                            SimulationChannelDescriptor** simulation_channels )
{
    if( mSimulationInitilized == false )
    {
        mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), &mSettings );
        mSimulationInitilized = true;
    }

    return mSimulationDataGenerator.GenerateSimulationData( minimum_sample_index, device_sample_rate, simulation_channels );
}

U32 JoybusAnalyzer::GetMinimumSampleRateHz()
{
    // Symbols are told apart by low periods a quarter bit long, so a quarter bit needs
    // enough samples to measure. This puts about 15 of them in the shortest one.
    return 16000000;
}

const char* JoybusAnalyzer::GetAnalyzerName() const
{
    return "Joybus";
}

const char* GetAnalyzerName()
{
    return "Joybus";
}

Analyzer* CreateAnalyzer()
{
    return new JoybusAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer )
{
    delete analyzer;
}