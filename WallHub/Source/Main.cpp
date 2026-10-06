#include "Hub.h"

class WallHubApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Wall Hub"; }
    const juce::String getApplicationVersion() override { return juce::String (HUB_VERSION); }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String&) override { window = std::make_unique<Window>(); }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    struct Window : juce::DocumentWindow
    {
        Window() : DocumentWindow ("Wall Hub", juce::Colour (0xff0b0c13), DocumentWindow::closeButton | DocumentWindow::minimiseButton)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new hub::HubComponent(), true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (WallHubApp)
