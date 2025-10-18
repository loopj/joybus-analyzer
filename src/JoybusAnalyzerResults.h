#pragma once

#include <AnalyzerResults.h>

class JoybusAnalyzer;
class JoybusAnalyzerSettings;

class JoybusAnalyzerResults : public AnalyzerResults
{
  public:
    JoybusAnalyzerResults( JoybusAnalyzer* analyzer, JoybusAnalyzerSettings* settings );
    virtual ~JoybusAnalyzerResults();

    virtual void GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base );
    virtual void GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id );

    virtual void GenerateFrameTabularText( U64 frame_index, DisplayBase display_base );
    virtual void GeneratePacketTabularText( U64 packet_id, DisplayBase display_base );
    virtual void GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base );

  protected:
    JoybusAnalyzerSettings* mSettings;
    JoybusAnalyzer* mAnalyzer;
};
