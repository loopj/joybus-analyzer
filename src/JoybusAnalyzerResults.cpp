#include <iostream>
#include <fstream>

#include <AnalyzerHelpers.h>

#include "JoybusAnalyzerResults.h"
#include "JoybusAnalyzer.h"
#include "JoybusAnalyzerSettings.h"

JoybusAnalyzerResults::JoybusAnalyzerResults( JoybusAnalyzer* analyzer, JoybusAnalyzerSettings* settings )
    : AnalyzerResults(), mSettings( settings ), mAnalyzer( analyzer )
{
}

JoybusAnalyzerResults::~JoybusAnalyzerResults()
{
}

void JoybusAnalyzerResults::GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base )
{
    ClearResultStrings();
    Frame frame = GetFrame( frame_index );

    const char* error_str = JoybusErrorName( ( JoybusError )frame.mData2 );

    // An error frame stands in for symbols that never became a byte, so it carries the
    // reason instead of a value
    if( frame.mType == JOYBUS_PHASE_ERROR )
    {
        AddResultString( "!" );
        AddResultString( "Error" );
        AddResultString( "Error: ", error_str );
        return;
    }

    char bubble_str[ 128 ];
    AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, bubble_str, 128 );
    AddResultString( bubble_str );

    // A byte the decoder had to guess at keeps its value and says what was wrong with it
    if( frame.mData2 != JOYBUS_ERROR_NONE )
        AddResultString( bubble_str, " (", error_str, ")" );
}

void JoybusAnalyzerResults::GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id )
{
    std::ofstream file_stream( file, std::ios::out );

    U64 trigger_sample = mAnalyzer->GetTriggerSample();
    U32 sample_rate = mAnalyzer->GetSampleRate();

    file_stream << "Time [s],Value,Error" << std::endl;

    U64 num_frames = GetNumFrames();
    for( U32 i = 0; i < num_frames; i++ )
    {
        Frame frame = GetFrame( i );

        char time_str[ 128 ];
        AnalyzerHelpers::GetTimeString( frame.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, 128 );

        char number_str[ 128 ];
        AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );

        // An error frame has no value to report, only the reason it has none
        if( frame.mType == JOYBUS_PHASE_ERROR )
        {
            file_stream << time_str << ",," << JoybusErrorName( ( JoybusError )frame.mData2 ) << std::endl;
        }
        else if( frame.mData2 != JOYBUS_ERROR_NONE )
        {
            file_stream << time_str << "," << number_str << "," << JoybusErrorName( ( JoybusError )frame.mData2 ) << std::endl;
        }
        else
        {
            file_stream << time_str << "," << number_str << "," << std::endl;
        }

        if( UpdateExportProgressAndCheckForCancel( i, num_frames ) == true )
        {
            file_stream.close();
            return;
        }
    }

    file_stream.close();
}

void JoybusAnalyzerResults::GenerateFrameTabularText( U64 frame_index, DisplayBase display_base )
{
#ifdef SUPPORTS_PROTOCOL_SEARCH
    Frame frame = GetFrame( frame_index );
    ClearTabularText();

    const char* error_str = JoybusErrorName( ( JoybusError )frame.mData2 );

    if( frame.mType == JOYBUS_PHASE_ERROR )
    {
        AddTabularText( "Error: ", error_str );
        return;
    }

    char number_str[ 128 ];
    AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );

    if( frame.mData2 != JOYBUS_ERROR_NONE )
    {
        AddTabularText( number_str, " (", error_str, ")" );
    }
    else
    {
        AddTabularText( number_str );
    }
#endif
}

void JoybusAnalyzerResults::GeneratePacketTabularText( U64 packet_id, DisplayBase display_base )
{
    // not supported
}

void JoybusAnalyzerResults::GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base )
{
    // not supported
}