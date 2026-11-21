#include <JuceHeader.h>
#include "ui/MainComponent.h"

namespace mcl { int runSelfTest(); }

class MusicCreativeLabApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        if (commandLine.contains ("--selftest"))
        {
            auto failures = mcl::runSelfTest();
            setApplicationReturnValue (failures == 0 ? 0 : 1);
            quit();
            return;
        }

        juce::LookAndFeel::setDefaultLookAndFeel (&theme);
        juce::PropertiesFile::Options o;
        o.applicationName = "MusicCreativeLab";
        o.filenameSuffix = ".settings";
        o.folderName = "MusicCreativeLab";
        o.osxLibrarySubFolder = "Application Support";
        properties.setStorageParameters (o);
        if (auto sc = juce::SystemStats::getEnvironmentVariable ("MCL_SCALE", {}); sc.isNotEmpty())
            juce::Desktop::getInstance().setGlobalScaleFactor (sc.getFloatValue());
        mainWindow = std::make_unique<MainWindow> (getApplicationName(), properties);
        openFromCommandLine (commandLine);
    }

    void openFromCommandLine (const juce::String& commandLine)
    {
        auto path = commandLine.trim().unquoted();
        if (path.endsWithIgnoreCase (".mcl") && mainWindow != nullptr)
            if (auto* mc = dynamic_cast<mcl::MainComponent*> (mainWindow->getContentComponent()))
                mc->openFile (juce::File (path));
    }

    void shutdown() override
    {
        mainWindow.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted (const juce::String& commandLine) override { openFromCommandLine (commandLine); }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, juce::ApplicationProperties& props)
            : DocumentWindow (name, mcl::Colours::bg, DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new mcl::MainComponent (props), true);
            setResizable (true, true);
            setResizeLimits (1200, 720, 10000, 10000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
           #if JUCE_WINDOWS
            setFullScreen (false);
           #endif
        }
        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

private:
    mcl::Theme theme;
    juce::ApplicationProperties properties;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (MusicCreativeLabApp)
