#pragma once

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>

class JoybusAnalyzerSettings : public AnalyzerSettings
{
  public:
    JoybusAnalyzerSettings();
    virtual ~JoybusAnalyzerSettings();

    virtual bool SetSettingsFromInterfaces();
    void UpdateInterfacesFromSettings();
    virtual void LoadSettings( const char* settings );
    virtual const char* SaveSettings();


    Channel mDataChannel;

  protected:
    AnalyzerSettingInterfaceChannel mDataChannelInterface;
};