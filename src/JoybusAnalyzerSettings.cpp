#include <AnalyzerHelpers.h>

#include "JoybusAnalyzerSettings.h"

JoybusAnalyzerSettings::JoybusAnalyzerSettings() : mDataChannel( UNDEFINED_CHANNEL ), mDataChannelInterface()
{
    mDataChannelInterface.SetTitleAndTooltip( "Data", "Joybus data line" );
    mDataChannelInterface.SetChannel( mDataChannel );

    AddInterface( &mDataChannelInterface );

    AddExportOption( 0, "Export as text/csv file" );
    AddExportExtension( 0, "text", "txt" );
    AddExportExtension( 0, "csv", "csv" );

    ClearChannels();
    AddChannel( mDataChannel, "Serial", false );
}

JoybusAnalyzerSettings::~JoybusAnalyzerSettings()
{
}

bool JoybusAnalyzerSettings::SetSettingsFromInterfaces()
{
    mDataChannel = mDataChannelInterface.GetChannel();

    ClearChannels();
    AddChannel( mDataChannel, "Joybus", true );

    return true;
}

void JoybusAnalyzerSettings::UpdateInterfacesFromSettings()
{
    mDataChannelInterface.SetChannel( mDataChannel );
}

void JoybusAnalyzerSettings::LoadSettings( const char* settings )
{
    SimpleArchive text_archive;
    text_archive.SetString( settings );

    text_archive >> mDataChannel;

    ClearChannels();
    AddChannel( mDataChannel, "Joybus", true );

    UpdateInterfacesFromSettings();
}

const char* JoybusAnalyzerSettings::SaveSettings()
{
    SimpleArchive text_archive;

    text_archive << mDataChannel;

    return SetReturnString( text_archive.GetString() );
}
