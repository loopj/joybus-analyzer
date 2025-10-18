#include <AnalyzerChannelData.h>
#include <AnalyzerHelpers.h>

#include "JoybusAnalyzer.h"
#include "JoybusAnalyzerSettings.h"
#include "JoybusProtocol.h"

// Timing thresholds suitable for both host and target modes, with some margin for error
#define DATA_BIT_MIDPOINT_NS 2500
#define DATA_BIT_MAX_LO_NS   5000
#define DATA_BIT_MAX_HI_NS   5000
#define STOP_BIT_MAX_LO_NS   2750

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

bool JoybusAnalyzer::GetBit( BitState& bit_state )
{
    // We're called after a falling edge
    U64 bit_start = mJoybus->GetSampleNumber();

    // Move to rising edge
    mJoybus->AdvanceToNextEdge();
    U64 bit_rise = mJoybus->GetSampleNumber();

    // The low period of data bits should 3.75us or less
    // If the line stays low for much longer, something is wrong
    U64 low_time = SamplesToNs( bit_rise - bit_start );
    if( low_time >= DATA_BIT_MAX_LO_NS )
        return false;

    // Save the bit value, based on the low time
    bit_state = low_time < DATA_BIT_MIDPOINT_NS ? BIT_HIGH : BIT_LOW;

    // Look for the next falling edge (without advancing)
    U64 bit_end = mJoybus->GetSampleOfNextEdge();

    // We always expect a falling edge for the next data/stop bit after each data bit
    // The high period of data bits should be 3.75us or less
    // If the line stays high for much longer, something is wrong
    U64 high_time = SamplesToNs( bit_end - bit_rise );
    if( high_time >= DATA_BIT_MAX_HI_NS )
        return false;

    // Add a zero/one marker in the middle of the bit
    U64 bit_mid = ( bit_start + bit_end ) / 2;
    AnalyzerResults::MarkerType marker = ( bit_state == BIT_LOW ) ? AnalyzerResults::Zero : AnalyzerResults::One;
    mResults->AddMarker( bit_mid, marker, mSettings.mDataChannel );

    return true;
}

bool JoybusAnalyzer::GetStopBit()
{
    // We're called after a falling edge
    U64 bit_start = mJoybus->GetSampleNumber();

    // Move to rising edge
    mJoybus->AdvanceToNextEdge();
    U64 bit_rise = mJoybus->GetSampleNumber();

    // The low period of stop bits should be 2us or less
    // If the line stays low for much longer, something is wrong
    if( SamplesToNs( bit_rise - bit_start ) > STOP_BIT_MAX_LO_NS )
        return false;

    // Add a stop marker in the middle of low period
    U64 bit_mid = ( bit_start + bit_rise ) / 2;
    mResults->AddMarker( bit_mid, AnalyzerResults::Stop, mSettings.mDataChannel );

    // Add a frame for the stop bit
    FrameV2 framev2;
    mResults->AddFrameV2( framev2, "stop", bit_start, mJoybus->GetSampleNumber() - 1 );

    return true;
}

bool JoybusAnalyzer::GetByte( U8& byte )
{
    U64 byte_start = mJoybus->GetSampleNumber();

    // Clear the byte buffer
    byte = 0;

    // Read 8 bits
    for( U8 i = 0; i < 8; i++ )
    {
        // Read the next bit value
        BitState bit;
        if( !GetBit( bit ) )
            return false;

        // Build the byte
        byte |= ( bit == BIT_HIGH ? 1 : 0 ) << ( 7 - i );

        // Move to the next falling edge
        mJoybus->AdvanceToNextEdge();
    }

    U64 byte_end = mJoybus->GetSampleNumber() - 1;

    // Add a bubble for the byte
    Frame frame;
    frame.mStartingSampleInclusive = byte_start;
    frame.mEndingSampleInclusive = byte_end;
    frame.mType = mCurrentPhase;
    frame.mData1 = byte;
    mResults->AddFrame( frame );

    // Add a frame for the byte
    FrameV2 framev2;
    framev2.AddByte( "data", byte );
    framev2.AddString( "type", GetPhaseName() );
    mResults->AddFrameV2( framev2, "byte", byte_start, byte_end );

    mResults->CommitResults();

    return true;
}

void JoybusAnalyzer::GetTransaction()
{
    // Buffer for command, arguments, and response
    U8 command;
    U8 args[ JOYBUS_BLOCK_SIZE - 1 ];
    U8 response[ JOYBUS_BLOCK_SIZE ];

    // Start at falling edge
    mJoybus->AdvanceToNextEdge();

    // Read the command type
    mCurrentPhase = JOYBUS_PHASE_COMMAND;
    bool result = GetByte( command );
    if( !result )
    {
        AdvanceToBusIdle();
        return;
    }

    // Skip over unknown commands
    if( !JoybusProtocol::CommandExists( command ) )
    {
        AdvanceToBusIdle();
        return;
    }

    // Read the argument bytes
    mCurrentPhase = JOYBUS_PHASE_ARGS;
    for( int i = 0; i < JoybusProtocol::GetCommandTxLength( command ) - 1; i++ )
    {
        bool result = GetByte( args[ i ] );
        if( !result )
        {
            AdvanceToBusIdle();
            return;
        }
    }

    // Read the command stop bit
    result = GetStopBit();
    if( !result )
    {
        AdvanceToBusIdle();
        return;
    }

    // We expect a delay between the end of a host stop bit and the start of the target reply
    // of somewhere between 2us and 100us. Any delay longer than 100us is considered a timeout.
    U64 reply_time = SamplesToNs( mJoybus->GetSampleOfNextEdge() - mJoybus->GetSampleNumber() );
    if( reply_time > JOYBUS_REPLY_TIMEOUT_NS )
    {
        AdvanceToBusIdle();
        return;
    }

    // Advance to the start of the reply (falling edge)
    mJoybus->AdvanceToNextEdge();

    // Read the response bytes
    mCurrentPhase = JOYBUS_PHASE_RESPONSE;
    for( int i = 0; i < JoybusProtocol::GetCommandRxLength( command ); i++ )
    {
        bool result = GetByte( response[ i ] );
        if( !result )
        {
            AdvanceToBusIdle();
            return;
        }
    }

    // Read the response stop bit
    result = GetStopBit();
    if( !result )
    {
        AdvanceToBusIdle();
        return;
    }
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
    return 2000000;
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