#ifndef JOYBUS_SIMULATION_DATA_GENERATOR
#define JOYBUS_SIMULATION_DATA_GENERATOR

#include <SimulationChannelDescriptor.h>
#include <string>
class JoybusAnalyzerSettings;

enum JoybusMode
{
    JOYBUS_MODE_HOST,
    JOYBUS_MODE_TARGET
};

class JoybusSimulationDataGenerator
{
  public:
    JoybusSimulationDataGenerator();
    ~JoybusSimulationDataGenerator();

    void Initialize( U32 simulation_sample_rate, JoybusAnalyzerSettings* settings );
    U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channel );

  protected:
    JoybusAnalyzerSettings* mSettings;
    U32 mSimulationSampleRateHz;
    SimulationChannelDescriptor mJoybusSimulationData;
    bool mControllerConnected;
    bool mControllerInitialized;

    U32 NsToSamples( U64 ns );

    void CreateBit( BitState bit_state, JoybusMode mode );
    void CreateStopBit( JoybusMode mode );
    void CreateByte( U8 data, JoybusMode mode );
    void CreateJoybusMessage( U8* bytes, U8 length, JoybusMode mode );

    void CreateIdentifyCommand();
    void CreateIdentifyResponse();
    void CreateGCNReadCommand( U8 analog_mode, U8 motor_state );
    void CreateGCNReadResponse();
    void CreateGCNReadOriginCommand();
    void CreateGCNReadOriginResponse();
};
#endif // JOYBUS_SIMULATION_DATA_GENERATOR