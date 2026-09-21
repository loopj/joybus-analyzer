#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "MockChannelData.h"
#include "MockResults.h"
#include "MockSettings.h"
#include "TestMacros.h"

#include "JoybusAnalyzer.h"

using namespace AnalyzerTest;

// The SDK test harness predates FrameV2, so the test build supplies it and records
// every frame the analyzer emits

class FrameV2Data
{
  public:
    std::map<std::string, std::string> fields;
};

struct CapturedFrame
{
    std::string type;
    U64 start;
    U64 end;
    std::map<std::string, std::string> fields;
};

static std::vector<CapturedFrame> gFramesV2;

FrameV2::FrameV2() : mInternals( new FrameV2Data )
{
}

FrameV2::~FrameV2()
{
    delete mInternals;
}

void FrameV2::AddString( const char* key, const char* value )
{
    mInternals->fields[ key ] = value;
}

void FrameV2::AddByte( const char* key, U8 value )
{
    char buf[ 16 ];
    snprintf( buf, sizeof( buf ), "0x%02X", value );
    mInternals->fields[ key ] = buf;
}

void FrameV2::AddDouble( const char*, double )
{
}

void FrameV2::AddInteger( const char*, S64 )
{
}

void FrameV2::AddBoolean( const char*, bool )
{
}

void FrameV2::AddByteArray( const char*, const U8*, U64 )
{
}

void AnalyzerResults::AddFrameV2( const FrameV2& frame, const char* type, U64 start, U64 end )
{
    CapturedFrame captured = { type, start, end, frame.mInternals->fields };
    gFramesV2.push_back( captured );
}

void Analyzer::UseFrameV2()
{
}

// Builds a Joybus waveform as a list of alternating high and low intervals, in seconds

class Waveform
{
  public:
    // Extends the idle high period that precedes the next low pulse
    void Gap( double seconds )
    {
        mTime += seconds;
    }

    // Emits one low pulse at the current time
    void Pulse( double low )
    {
        mEdges.push_back( mTime );
        mTime += low;
        mEdges.push_back( mTime );
    }

    // Emits eight data symbols, most significant bit first
    void Byte( U8 value, double bit_time )
    {
        for( int i = 7; i >= 0; i-- )
        {
            double low = ( value & ( 1 << i ) ) ? bit_time / 4 : ( bit_time * 3 ) / 4;
            Pulse( low );
            Gap( bit_time - low );
        }
    }

    // Emits a whole transmission, terminated by a stop bit of the given width
    void Transmission( const std::vector<U8>& bytes, double bit_time, int stop_quarters )
    {
        for( size_t i = 0; i < bytes.size(); i++ )
            Byte( bytes[ i ], bit_time );

        Pulse( ( bit_time * stop_quarters ) / 4 );
    }

    // Displaces every edge on its own by up to the given amount, the way comparator
    // noise and slew do, rather than shifting the whole waveform
    void Jitter( double peak, U32 seed )
    {
        U32 state = seed | 1;
        for( size_t i = 0; i < mEdges.size(); i++ )
        {
            state = state * 1664525u + 1013904223u;
            double unit = ( ( state >> 8 ) & 0xFFFF ) / 65535.0;
            mEdges[ i ] += ( unit * 2.0 - 1.0 ) * peak;
        }
    }

    // The line starts high, so the intervals alternate high, low, high, low
    std::vector<double> Intervals() const
    {
        std::vector<double> intervals;
        double previous = 0.0;
        for( size_t i = 0; i < mEdges.size(); i++ )
        {
            intervals.push_back( mEdges[ i ] - previous );
            previous = mEdges[ i ];
        }

        return intervals;
    }

  private:
    std::vector<double> mEdges;
    double mTime = 0.0;
};

// Runs the analyzer over a waveform and returns everything it decoded

struct Decoded
{
    std::vector<Frame> frames;
    std::vector<Frame> errors;
    std::vector<CapturedFrame> framesV2;
};

static const double kBusIdle = 200e-6;

static Decoded Run( const Waveform& waveform, U64 sample_rate_hz = 50000000 )
{
    gFramesV2.clear();

    Instance plugin( "Joybus" );
    plugin.SetSampleRate( sample_rate_hz );

    Channel channel( 0, 0, DIGITAL_CHANNEL );
    MockSettings* settings = MockSettings::MockFromSettings( plugin.GetSettings() );
    settings->GetSetting( "Data" )->mChannel = channel;
    plugin.GetSettings()->SetSettingsFromInterfaces();

    MockChannelData data( &plugin );
    data.TestSetInitialBitState( BIT_HIGH );
    data.TestAppendIntervals( sample_rate_hz, 0.0, waveform.Intervals() );
    data.ResetCurrentSample();
    plugin.SetChannelData( channel, &data );

    plugin.RunAnalyzerWorker();

    // The bytes and the errors go into the same bubble lane, so they are split apart
    // here and checked separately
    Decoded decoded;
    MockResultData* results = MockResultData::MockFromResults( plugin.GetResults() );
    for( U64 i = 0; i < results->TotalFrameCount(); i++ )
    {
        Frame frame = results->GetFrame( i );
        if( frame.mType == JOYBUS_PHASE_ERROR )
        {
            decoded.errors.push_back( frame );
        }
        else
        {
            decoded.frames.push_back( frame );
        }
    }

    decoded.framesV2 = gFramesV2;

    return decoded;
}

// Checks the decoded bytes and their phases against what was transmitted

static void VerifyBytes( const Decoded& decoded, const std::vector<U8>& command, const std::vector<U8>& response )
{
    TEST_VERIFY_EQ( decoded.frames.size(), command.size() + response.size() );

    for( size_t i = 0; i < command.size(); i++ )
    {
        TEST_VERIFY_EQ( decoded.frames[ i ].mData1, command[ i ] );
        TEST_VERIFY_EQ( ( int )decoded.frames[ i ].mType, ( int )( i == 0 ? JOYBUS_PHASE_COMMAND : JOYBUS_PHASE_ARGS ) );
    }

    for( size_t i = 0; i < response.size(); i++ )
    {
        const Frame& frame = decoded.frames[ command.size() + i ];
        TEST_VERIFY_EQ( frame.mData1, response[ i ] );
        TEST_VERIFY_EQ( ( int )frame.mType, ( int )JOYBUS_PHASE_RESPONSE );
    }

    // A waveform this clean leaves nothing for the decoder to complain about
    for( size_t i = 0; i < decoded.frames.size(); i++ )
    {
        TEST_VERIFY_EQ( ( int )decoded.frames[ i ].mData2, ( int )JOYBUS_ERROR_NONE );
        TEST_VERIFY_EQ( ( int )decoded.frames[ i ].mFlags, 0 );
    }
}

// Emits one symbol held low for the given fraction of a bit time

static void Symbol( Waveform& waveform, double bit_time, double fraction )
{
    double low = bit_time * fraction;
    waveform.Pulse( low );
    waveform.Gap( bit_time - low );
}

// Returns the reasons the analyzer gave for the runs it could not read, in order

static std::vector<std::string> Errors( const Decoded& decoded )
{
    std::vector<std::string> reasons;
    for( size_t i = 0; i < decoded.framesV2.size(); i++ )
    {
        if( decoded.framesV2[ i ].type == "error" )
            reasons.push_back( decoded.framesV2[ i ].fields.at( "error" ) );
    }

    return reasons;
}

// Returns the byte frames the analyzer marked, as the reason it gave for each

static std::vector<std::string> ByteErrors( const Decoded& decoded )
{
    std::vector<std::string> reasons;
    for( size_t i = 0; i < decoded.framesV2.size(); i++ )
    {
        if( decoded.framesV2[ i ].type == "byte" && decoded.framesV2[ i ].fields.count( "error" ) )
            reasons.push_back( decoded.framesV2[ i ].fields.at( "error" ) );
    }

    return reasons;
}

// Returns the stop bit types the analyzer reported, in order

static std::vector<std::string> StopBits( const Decoded& decoded )
{
    std::vector<std::string> types;
    for( size_t i = 0; i < decoded.framesV2.size(); i++ )
    {
        if( decoded.framesV2[ i ].type == "stop" )
            types.push_back( decoded.framesV2[ i ].fields.at( "type" ) );
    }

    return types;
}

// Measured host and target rates, and the turnaround each target was measured at. The
// turnaround runs from the host releasing the line at the end of its stop bit to the
// first falling edge of the reply.

struct DevicePair
{
    const char* name;
    double host_hz;
    double target_hz;
    double turnaround;
};

static const DevicePair kDevicePairs[] = {
    { "N64 console and N64 controller", 244141, 253160, 3.22e-6 },
    { "N64 console and N64 VRU", 244141, 250810, 3.14e-6 },
    { "GameCube console and GameCube controller", 202500, 251570, 3.45e-6 },
    { "GameCube console and WaveBird receiver", 202500, 225370, 3.95e-6 },
    { "GameCube console and Game Boy Advance cable", 202500, 262320, 6.59e-6 },
    { "Wii console and GameCube controller", 202500, 251570, 3.45e-6 },
    { "N64 console and N64 cartridge, one clock between them", 244141, 244141, 3.22e-6 },
};

// A command and its reply, at the rates and turnaround of one real pair of devices

static void TestDevicePair( const DevicePair& pair, const std::vector<U8>& command, const std::vector<U8>& response )
{
    double host_bit = 1.0 / pair.host_hz;
    double target_bit = 1.0 / pair.target_hz;

    Waveform waveform;
    waveform.Gap( kBusIdle );
    waveform.Transmission( command, host_bit, 1 );
    waveform.Gap( pair.turnaround );
    waveform.Transmission( response, target_bit, 2 );
    waveform.Gap( kBusIdle );
    waveform.Pulse( 1e-6 );
    waveform.Gap( kBusIdle );

    Decoded decoded = Run( waveform );
    VerifyBytes( decoded, command, response );

    std::vector<std::string> stops = StopBits( decoded );
    TEST_VERIFY_EQ( stops.size(), 2u );
    TEST_VERIFY_EQ( stops[ 0 ], std::string( "host" ) );
    TEST_VERIFY_EQ( stops[ 1 ], std::string( "target" ) );
}

static void TestRealDevicePairs()
{
    std::vector<U8> read_command = { 0x40, 0x03, 0x00 };
    std::vector<U8> read_response = { 0x00, 0x80, 0x7F, 0x81, 0x80, 0x20, 0x1F, 0x00 };

    // An opcode whose leading bit could be mistaken for a stop bit if the split were
    // guessed rather than measured
    std::vector<U8> identify_command = { 0x00 };
    std::vector<U8> identify_response = { 0x09, 0x00, 0x03 };

    for( size_t i = 0; i < sizeof( kDevicePairs ) / sizeof( kDevicePairs[ 0 ] ); i++ )
    {
        std::cerr << "  " << kDevicePairs[ i ].name << std::endl;
        TestDevicePair( kDevicePairs[ i ], read_command, read_response );
        TestDevicePair( kDevicePairs[ i ], identify_command, identify_response );
    }
}

// The turnaround a target is measured at moves around from one reply to the next, so
// the split has to hold across the whole range one was seen over

static void TestTurnaroundSpread()
{
    std::vector<U8> command = { 0x40, 0x03, 0x00 };
    std::vector<U8> response = { 0x00, 0x80, 0x7F, 0x81, 0x80, 0x20, 0x1F, 0x00 };

    // The N64 controller is the hardest of the measured targets, being closest to the
    // console's own rate and replying closest to a bit time after the stop bit
    std::cerr << "  N64 controller across its measured turnaround range" << std::endl;
    for( double turnaround = 3.15e-6; turnaround <= 3.63e-6; turnaround += 0.02e-6 )
    {
        DevicePair pair = { "N64 console and N64 controller", 244141, 253160, turnaround };
        TestDevicePair( pair, command, response );
    }
}

// A command nothing replies to leaves the line idle after the host stop bit

static void TestNoResponse()
{
    std::vector<U8> command = { 0x00 };

    Waveform waveform;
    waveform.Gap( kBusIdle );
    waveform.Transmission( command, 1.0 / 202500, 1 );
    waveform.Gap( kBusIdle );
    waveform.Pulse( 1e-6 );
    waveform.Gap( kBusIdle );

    Decoded decoded = Run( waveform );
    VerifyBytes( decoded, command, {} );

    std::vector<std::string> stops = StopBits( decoded );
    TEST_VERIFY_EQ( stops.size(), 1u );
    TEST_VERIFY_EQ( stops[ 0 ], std::string( "host" ) );
}

// An opcode the analyzer has never heard of decodes like any other

static void TestUnknownCommand()
{
    std::vector<U8> command = { 0xAB, 0xCD };
    std::vector<U8> response = { 0x12, 0x34, 0x56 };

    DevicePair pair = { "unknown opcode", 202500, 251570, 3.45e-6 };
    TestDevicePair( pair, command, response );
}

// Back to back transactions with no bus idle between them

static void TestConsecutiveTransactions()
{
    std::vector<U8> command = { 0x41 };
    std::vector<U8> response = { 0x00, 0x80, 0x80, 0x80, 0x80, 0x1F, 0x1F, 0x00, 0x02, 0x02 };

    Waveform waveform;
    waveform.Gap( kBusIdle );
    for( int i = 0; i < 3; i++ )
    {
        waveform.Transmission( command, 1.0 / 202500, 1 );
        waveform.Gap( 3.45e-6 );
        waveform.Transmission( response, 1.0 / 251570, 2 );
        waveform.Gap( 50e-6 );
    }
    waveform.Gap( kBusIdle );
    waveform.Pulse( 1e-6 );
    waveform.Gap( kBusIdle );

    Decoded decoded = Run( waveform );
    TEST_VERIFY_EQ( decoded.frames.size(), 3 * ( command.size() + response.size() ) );
    TEST_VERIFY_EQ( StopBits( decoded ).size(), 6u );
}

// A target on the host's own clock that starts its reply exactly one bit time after the
// stop bit's falling edge leaves nothing at all to tell the stop bit from a data one

static void TestReplyAtOneBitTime()
{
    const double bit_time = 4000e-9;

    std::vector<U8> command = { 0x40, 0x03, 0x00 };
    std::vector<U8> response = { 0x00, 0x80, 0x7F, 0x81, 0x80, 0x20, 0x1F, 0x00 };

    Waveform waveform;
    waveform.Gap( kBusIdle );
    waveform.Transmission( command, bit_time, 1 );
    waveform.Gap( ( bit_time * 3 ) / 4 );
    waveform.Transmission( response, bit_time, 2 );
    waveform.Gap( kBusIdle );
    waveform.Pulse( 1e-6 );
    waveform.Gap( kBusIdle );

    Decoded decoded = Run( waveform );
    VerifyBytes( decoded, command, response );
}

// The same case, but with an argument byte whose leading bit could just as well have
// been the stop bit. Nothing in the waveform separates the two, so the analyzer takes
// the first byte boundary the host could have stopped on and gets it wrong.

static void TestReplyAtOneBitTimeWorstCase()
{
    const double bit_time = 4000e-9;

    std::vector<U8> command = { 0x40, 0x83, 0x00 };
    std::vector<U8> response = { 0x01, 0x02, 0x03 };

    Waveform waveform;
    waveform.Gap( kBusIdle );
    waveform.Transmission( command, bit_time, 1 );
    waveform.Gap( ( bit_time * 3 ) / 4 );
    waveform.Transmission( response, bit_time, 2 );
    waveform.Gap( kBusIdle );
    waveform.Pulse( 1e-6 );
    waveform.Gap( kBusIdle );

    Decoded decoded = Run( waveform );

    // The opcode is still right, and everything past the guessed split is shifted by a bit
    TEST_VERIFY_EQ( decoded.frames.size(), 6u );
    TEST_VERIFY_EQ( decoded.frames[ 0 ].mData1, command[ 0 ] );
    TEST_VERIFY_EQ( ( int )decoded.frames[ 0 ].mType, ( int )JOYBUS_PHASE_COMMAND );
    TEST_VERIFY_EQ( ( int )decoded.frames[ 1 ].mType, ( int )JOYBUS_PHASE_RESPONSE );
}

// Reports whether a pair decodes cleanly, without aborting the run on a mismatch

static bool DecodesCleanly( double host_hz, double target_hz, double turnaround, const std::vector<U8>& command,
                            const std::vector<U8>& response, double jitter = 0.0, U32 seed = 1 )
{
    Waveform waveform;
    waveform.Gap( kBusIdle );
    waveform.Transmission( command, 1.0 / host_hz, 1 );
    waveform.Gap( turnaround );
    waveform.Transmission( response, 1.0 / target_hz, 2 );
    waveform.Gap( kBusIdle );
    waveform.Pulse( 1e-6 );
    waveform.Gap( kBusIdle );
    waveform.Jitter( jitter, seed );

    Decoded decoded = Run( waveform );
    if( decoded.frames.size() != command.size() + response.size() )
        return false;

    for( size_t i = 0; i < command.size(); i++ )
    {
        if( decoded.frames[ i ].mData1 != command[ i ] )
            return false;
        if( decoded.frames[ i ].mType != ( i == 0 ? JOYBUS_PHASE_COMMAND : JOYBUS_PHASE_ARGS ) )
            return false;
    }

    for( size_t i = 0; i < response.size(); i++ )
    {
        if( decoded.frames[ command.size() + i ].mData1 != response[ i ] )
            return false;
        if( decoded.frames[ command.size() + i ].mType != JOYBUS_PHASE_RESPONSE )
            return false;
    }

    return true;
}

// The one waveform the analyzer cannot read needs two things at once: the reply's first
// falling edge landing on the sample the host's next data bit would have used, and the
// target running the host's own clock. Either one alone resolves the split.

static void TestAmbiguityBoundary()
{
    std::vector<U8> command = { 0x40, 0x83, 0x00 };
    std::vector<U8> response = { 0x00, 0x80, 0x7F, 0x81, 0x80, 0x20, 0x1F, 0x00 };

    const double host_hz = 250000;
    const double on_grid = 3.0 / ( host_hz * 4 );

    // Both at once, which is the waveform that carries no answer
    TEST_VERIFY( !DecodesCleanly( host_hz, host_hz, on_grid, command, response ) );

    // A reply a single sample either side of the grid is enough
    TEST_VERIFY( DecodesCleanly( host_hz, host_hz, on_grid - 25e-9, command, response ) );
    TEST_VERIFY( DecodesCleanly( host_hz, host_hz, on_grid + 25e-9, command, response ) );

    // So is a target a third of a per mille off the host's rate
    TEST_VERIFY( DecodesCleanly( host_hz, host_hz * 1.0003, on_grid, command, response ) );
    TEST_VERIFY( DecodesCleanly( host_hz, host_hz * 0.9997, on_grid, command, response ) );
}

// Edge noise on the hardest measured pair. The symbols themselves only have a quarter
// bit of margin between them, so this is well short of where classifying a low period
// stops working, but it covers far more than a crystal clocked device puts out.

static void TestJitterTolerance()
{
    std::vector<U8> command = { 0x40, 0x83, 0x00 };
    std::vector<U8> response = { 0x00, 0x80, 0x7F, 0x81, 0x80, 0x20, 0x1F, 0x00 };

    for( U32 seed = 1; seed <= 200; seed++ )
        TEST_VERIFY( DecodesCleanly( 244141, 253160, 3.22e-6, command, response, 150e-9, seed ) );
}

// Appends bus idle and one clean byte, so the decoder can see the line go quiet and
// come back. An unreadable run is only reported once that has happened.

static void AppendIdleTransaction( Waveform& waveform )
{
    const double bit_time = 4000e-9;

    waveform.Gap( kBusIdle );
    waveform.Transmission( { 0x00 }, bit_time, 1 );
    waveform.Gap( kBusIdle );
}

// Edges too close together for anything on the bus to have sent them

static void TestBitTimeError()
{
    const double bit_time = 2000e-9;

    Waveform waveform;
    waveform.Gap( kBusIdle );
    for( int i = 0; i < 8; i++ )
        Symbol( waveform, bit_time, 0.25 );
    AppendIdleTransaction( waveform );

    Decoded decoded = Run( waveform );
    TEST_VERIFY_EQ( decoded.frames.size(), 1u );
    TEST_VERIFY_EQ( decoded.errors.size(), 1u );
    TEST_VERIFY_EQ( Errors( decoded )[ 0 ], std::string( "bit time" ) );
}

// A run at a believable rate that holds no whole number of bytes

static void TestFramingError()
{
    const double bit_time = 4000e-9;

    Waveform waveform;
    waveform.Gap( kBusIdle );
    for( int i = 0; i < 4; i++ )
        Symbol( waveform, bit_time, 0.25 );
    AppendIdleTransaction( waveform );

    Decoded decoded = Run( waveform );
    TEST_VERIFY_EQ( decoded.frames.size(), 1u );
    TEST_VERIFY_EQ( decoded.errors.size(), 1u );
    TEST_VERIFY_EQ( Errors( decoded )[ 0 ], std::string( "framing" ) );
}

// A data bit held low for longer than any symbol still decodes, to whichever symbol it
// sits nearest, and the byte carrying it says so

static void TestInvalidSymbol()
{
    const double bit_time = 4000e-9;

    Waveform waveform;
    waveform.Gap( kBusIdle );
    for( int i = 0; i < 7; i++ )
        Symbol( waveform, bit_time, 0.25 );
    Symbol( waveform, bit_time, 0.95 );
    Symbol( waveform, bit_time, 0.25 );
    waveform.Gap( kBusIdle );

    Decoded decoded = Run( waveform );
    TEST_VERIFY_EQ( decoded.frames.size(), 1u );
    TEST_VERIFY_EQ( decoded.frames[ 0 ].mData1, 0xFE );
    TEST_VERIFY_EQ( ( int )decoded.frames[ 0 ].mData2, ( int )JOYBUS_ERROR_SYMBOL );
    TEST_VERIFY( decoded.frames[ 0 ].HasFlag( DISPLAY_AS_ERROR_FLAG ) );
    TEST_VERIFY_EQ( ByteErrors( decoded ).size(), 1u );
    TEST_VERIFY_EQ( ByteErrors( decoded )[ 0 ], std::string( "symbol" ) );
}

// A data bit sitting halfway between the one and zero widths is read as the nearer of
// the two, which is worth a warning rather than an error

static void TestMarginWarning()
{
    const double bit_time = 4000e-9;

    Waveform waveform;
    waveform.Gap( kBusIdle );
    for( int i = 0; i < 7; i++ )
        Symbol( waveform, bit_time, 0.25 );
    Symbol( waveform, bit_time, 0.5 );
    Symbol( waveform, bit_time, 0.25 );
    waveform.Gap( kBusIdle );

    Decoded decoded = Run( waveform );
    TEST_VERIFY_EQ( decoded.frames.size(), 1u );
    TEST_VERIFY_EQ( ( int )decoded.frames[ 0 ].mData2, ( int )JOYBUS_ERROR_MARGIN );
    TEST_VERIFY( decoded.frames[ 0 ].HasFlag( DISPLAY_AS_WARNING_FLAG ) );
    TEST_VERIFY_EQ( ByteErrors( decoded )[ 0 ], std::string( "margin" ) );
}

// A run that ends on a data zero rather than on a stop bit either end could have sent

static void TestBadStopBit()
{
    const double bit_time = 4000e-9;

    Waveform waveform;
    waveform.Gap( kBusIdle );
    for( int i = 0; i < 8; i++ )
        Symbol( waveform, bit_time, 0.25 );
    Symbol( waveform, bit_time, 0.75 );
    waveform.Gap( kBusIdle );

    Decoded decoded = Run( waveform );
    TEST_VERIFY_EQ( decoded.frames.size(), 1u );
    TEST_VERIFY_EQ( decoded.frames[ 0 ].mData1, 0xFF );
    TEST_VERIFY_EQ( decoded.errors.size(), 1u );
    TEST_VERIFY_EQ( Errors( decoded )[ 0 ], std::string( "stop bit" ) );
}

// A capture starts and ends wherever the trigger put it, so the transmissions at either
// end are cut off through no fault of the bus. Neither one is an error.

static void TestCaptureBoundaries()
{
    const double bit_time = 4000e-9;
    std::vector<U8> command = { 0x40, 0x03, 0x00 };
    std::vector<U8> response = { 0x00, 0x80, 0x7F, 0x81, 0x80, 0x20, 0x1F, 0x00 };

    // Every offset into a transmission the capture could have opened on
    for( int skip = 0; skip < 8; skip++ )
    {
        Waveform waveform;

        // The tail of a reply whose command was never captured
        waveform.Gap( 20e-6 );
        for( int i = skip; i < 8; i++ )
            Symbol( waveform, bit_time, 0.25 );
        Symbol( waveform, bit_time, 0.5 );

        // One whole transaction, which is all the capture holds enough of to read
        waveform.Gap( kBusIdle );
        waveform.Transmission( command, bit_time, 1 );
        waveform.Gap( 3.45e-6 );
        waveform.Transmission( response, bit_time * 0.98, 2 );

        // The head of a command the capture closed on
        waveform.Gap( kBusIdle );
        for( int i = 0; i < 5; i++ )
            Symbol( waveform, bit_time, 0.25 );

        Decoded decoded = Run( waveform );
        VerifyBytes( decoded, command, response );
        TEST_VERIFY_EQ( decoded.errors.size(), 0u );
    }
}

int main()
{
    TestRealDevicePairs();
    TestTurnaroundSpread();
    TestNoResponse();
    TestUnknownCommand();
    TestConsecutiveTransactions();
    TestReplyAtOneBitTime();
    TestReplyAtOneBitTimeWorstCase();
    TestAmbiguityBoundary();
    TestJitterTolerance();
    TestCaptureBoundaries();
    TestBitTimeError();
    TestFramingError();
    TestInvalidSymbol();
    TestMarginWarning();
    TestBadStopBit();

    std::cout << "all tests passed" << std::endl;

    return 0;
}
