#pragma once

#include <vector>

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

// Symbol kinds, told apart by how long the line is held low
enum JoybusSymbolKind
{
    JOYBUS_SYMBOL_ONE,         // one quarter bit, a data one or a host stop bit
    JOYBUS_SYMBOL_TARGET_STOP, // two quarter bits
    JOYBUS_SYMBOL_ZERO,        // three quarter bits
    JOYBUS_SYMBOL_INVALID,     // a low period no symbol accounts for
};

// The edges one symbol was measured from
struct JoybusSymbol
{
    U64 fall;      // leading falling edge
    U64 rise;      // rising edge ending the low period
    U64 next_fall; // following falling edge, only valid when terminal is false
    bool terminal; // nothing pulled the line low again, so the transmitter let go
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

    JoybusSymbolKind ClassifySymbol( const JoybusSymbol& symbol, double bit_time );
    void FitGrid( const std::vector<JoybusSymbol>& symbols, size_t first, size_t last, double& period, double& error );

    bool GetSymbolRun( std::vector<JoybusSymbol>& symbols );
    size_t FindStopBit( const std::vector<JoybusSymbol>& symbols );

    void AddByteFrame( U8 byte, U64 start, U64 end );
    void AddStopBitFrame( const JoybusSymbol& symbol, double bit_time );

    void EmitTransmission( const std::vector<JoybusSymbol>& symbols, size_t first, size_t stop );
    void GetTransaction();

    const char* GetPhaseName();
};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer();
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );
