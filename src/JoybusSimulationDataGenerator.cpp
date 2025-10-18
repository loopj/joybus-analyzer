#include "JoybusSimulationDataGenerator.h"
#include "JoybusAnalyzerSettings.h"

#include <AnalyzerHelpers.h>

JoybusSimulationDataGenerator::JoybusSimulationDataGenerator()
{
}

JoybusSimulationDataGenerator::~JoybusSimulationDataGenerator()
{
}

void JoybusSimulationDataGenerator::Initialize( U32 simulation_sample_rate, JoybusAnalyzerSettings* settings )
{
    mSimulationSampleRateHz = simulation_sample_rate;
    mSettings = settings;

    mJoybusSimulationData.SetChannel( mSettings->mDataChannel );
    mJoybusSimulationData.SetSampleRate( simulation_sample_rate );
    mJoybusSimulationData.SetInitialBitState( BIT_HIGH );

    mJoybusSimulationData.Advance( NsToSamples( 1000000 ) );

    mControllerConnected = true;
    mControllerInitialized = false;
}

U32 JoybusSimulationDataGenerator::GenerateSimulationData( U64 largest_sample_requested, U32 sample_rate,
                                                           SimulationChannelDescriptor** simulation_channel )
{
    U64 adjusted_largest_sample_requested =
        AnalyzerHelpers::AdjustSimulationTargetSample( largest_sample_requested, sample_rate, mSimulationSampleRateHz );

    U8 poll_frequency_hz = 60;

    while( mJoybusSimulationData.GetCurrentSampleNumber() < adjusted_largest_sample_requested )
    {
        U64 starting_sample = mJoybusSimulationData.GetCurrentSampleNumber();

        if( mControllerConnected )
        {
            // GameCube controller initialization sequence
            if( !mControllerInitialized )
            {
                // Identify, with response
                CreateIdentifyCommand();
                CreateIdentifyResponse();
                mJoybusSimulationData.Advance( NsToSamples( 30000 ) );

                // GameCube read origin
                CreateGCNReadOriginCommand();
                CreateGCNReadOriginResponse();

                mControllerInitialized = true;
            }
            else
            {
                // Randomly disconnect the controller
                if( rand() % 50 == 0 )
                {
                    mControllerConnected = false;
                    mControllerInitialized = false;
                }

                // Polled "read"
                CreateGCNReadCommand( 0x03, 0x00 );
                if( mControllerConnected )
                    CreateGCNReadResponse();
            }
        }
        else
        {
            // Polled "identify" (no response)
            CreateIdentifyCommand();

            // Reconnect the controller after a few tries
            if( rand() % 5 == 0 )
                mControllerConnected = true;
        }

        // Ensure a full 16.666ms polling interval
        U64 ending_sample = mJoybusSimulationData.GetCurrentSampleNumber();
        U64 duration_samples = ending_sample - starting_sample;
        mJoybusSimulationData.Advance( NsToSamples( 1000000000 / poll_frequency_hz ) - duration_samples );
    }

    *simulation_channel = &mJoybusSimulationData;
    return 1;
}

U32 JoybusSimulationDataGenerator::NsToSamples( U64 ns )
{
    return static_cast<U32>( mSimulationSampleRateHz * ns / 1000000000 );
}

void JoybusSimulationDataGenerator::CreateBit( BitState bit_state, JoybusMode mode )
{
    if( bit_state == BIT_LOW )
    {
        mJoybusSimulationData.Transition();
        mJoybusSimulationData.Advance( NsToSamples( mode == JOYBUS_MODE_HOST ? 3750 : 3000 ) );
        mJoybusSimulationData.Transition();
        mJoybusSimulationData.Advance( NsToSamples( mode == JOYBUS_MODE_HOST ? 1250 : 1000 ) );
    }
    else
    {
        mJoybusSimulationData.Transition();
        mJoybusSimulationData.Advance( NsToSamples( mode == JOYBUS_MODE_HOST ? 1250 : 1000 ) );
        mJoybusSimulationData.Transition();
        mJoybusSimulationData.Advance( NsToSamples( mode == JOYBUS_MODE_HOST ? 3750 : 3000 ) );
    }
}


void JoybusSimulationDataGenerator::CreateStopBit( JoybusMode mode )
{
    mJoybusSimulationData.Transition();
    mJoybusSimulationData.Advance( NsToSamples( mode == JOYBUS_MODE_HOST ? 1250 : 2000 ) );
    mJoybusSimulationData.Transition();
    mJoybusSimulationData.Advance( NsToSamples( 2000 ) );
}


void JoybusSimulationDataGenerator::CreateByte( U8 data, JoybusMode mode )
{
    U8 mask = ( 1 << 7 );
    for( int i = 0; i < 8; i++ )
    {
        CreateBit( ( data & mask ) ? BIT_HIGH : BIT_LOW, mode );
        mask >>= 1;
    }
}

void JoybusSimulationDataGenerator::CreateJoybusMessage( U8* bytes, U8 length, JoybusMode mode )
{
    for( U8 i = 0; i < length; i++ )
    {
        CreateByte( bytes[ i ], mode );
    }
    CreateStopBit( mode );
}

void JoybusSimulationDataGenerator::CreateIdentifyCommand()
{
    // Identify request: host sends 0x00
    U8 identify0[ 1 ] = { 0x00 };
    CreateJoybusMessage( identify0, 1, JOYBUS_MODE_HOST );
}

void JoybusSimulationDataGenerator::CreateIdentifyResponse()
{
    // Identify response from target: 0x09 0x00 0x20
    U8 resp[ 3 ] = { 0x09, 0x00, 0x20 };
    CreateJoybusMessage( resp, 3, JOYBUS_MODE_TARGET );
}

void JoybusSimulationDataGenerator::CreateGCNReadCommand( U8 analog_mode, U8 motor_state )
{
    U8 request[ 3 ] = { 0x40, analog_mode, motor_state };
    CreateJoybusMessage( request, 3, JOYBUS_MODE_HOST );
}

void JoybusSimulationDataGenerator::CreateGCNReadResponse()
{
    U8 response[ 8 ] = { 0 };
    for( int i = 2; i < 8; ++i )
        response[ i ] = ( U8 )( rand() & 0xFF );

    CreateJoybusMessage( response, 8, JOYBUS_MODE_TARGET );
}

void JoybusSimulationDataGenerator::CreateGCNReadOriginCommand()
{
    U8 cmd[ 1 ] = { 0x41 };
    CreateJoybusMessage( cmd, 1, JOYBUS_MODE_HOST );
}

void JoybusSimulationDataGenerator::CreateGCNReadOriginResponse()
{
    U8 response[ 10 ] = { 0 };

    // Sticks: slight jitter around 0x80
    for( int i = 2; i <= 5; ++i )
        response[ i ] = ( U8 )( 0x80 + ( rand() % 17 ) - 8 );

    // Triggers: slight jitter between 0 and 20
    response[ 6 ] = ( U8 )( rand() % 21 );
    response[ 7 ] = ( U8 )( rand() % 21 );

    CreateJoybusMessage( response, 10, JOYBUS_MODE_TARGET );
}
