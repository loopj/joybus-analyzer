#pragma once

#include <Analyzer.h>

#include "JoybusAnalyzerSettings.h"
#include "JoybusAnalyzerResults.h"
#include "JoybusSimulationDataGenerator.h"

enum JoybusPhase
{
    JOYBUS_PHASE_IDLE,
    JOYBUS_PHASE_COMMAND,
    JOYBUS_PHASE_ARGS,
    JOYBUS_PHASE_RESPONSE,
};

class ANALYZER_EXPORT JoybusAnalyzer : public Analyzer2
{
  public:
    JoybusAnalyzer();
    virtual ~JoybusAnalyzer();

    virtual void SetupResults();
    virtual void WorkerThread();

    virtual U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels );
    virtual U32 GetMinimumSampleRateHz();

    virtual const char* GetAnalyzerName() const;
    virtual bool NeedsRerun();

  protected:
    JoybusAnalyzerSettings mSettings;
    std::unique_ptr<JoybusAnalyzerResults> mResults;
    AnalyzerChannelData* mJoybus;

    JoybusSimulationDataGenerator mSimulationDataGenerator;
    bool mSimulationInitilized;
    U32 mSampleRateHz;
    JoybusPhase mCurrentPhase;

    U64 SamplesToNs( U64 samples );

    void AdvanceToBusIdle();

    bool GetBit( BitState& bit_state );
    bool GetStopBit();
    bool GetByte( U8& byte );
    void GetTransaction();

    const char* GetPhaseName();
};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer();
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );
