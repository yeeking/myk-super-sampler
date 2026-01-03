#pragma once

#include "SuperSamplerProcessor.h"
#include "Segment14Geometry.h"
#include <JuceHeader.h>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class SuperSamplerEditor final : public juce::AudioProcessorEditor,
                           public juce::OpenGLRenderer,
                           public juce::Timer,
                           public juce::KeyListener
{
public:
    explicit SuperSamplerEditor (SuperSamplerProcessor&);
    ~SuperSamplerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void timerCallback() override;

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;

    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    void updateUIFromProcessor(const juce::var& payload);

private:
    enum class Action
    {
        None,
        Add,
        Load,
        Trigger,
        Low,
        High,
        Gain,
        Waveform
    };

    struct PlayerUIState
    {
        int id = 0;
        int midiLow = 36;
        int midiHigh = 60;
        float gain = 1.0f;
        bool isPlaying = false;
        juce::String status;
        juce::String fileName;
        juce::String filePath;
        std::vector<float> waveformPoints;
    };

    struct CellVisualState
    {
        bool isSelected = false;
        bool isEditing = false;
        bool isActive = false;
        bool isDisabled = false;
        float glow = 0.0f;
    };

    struct CellInfo
    {
        Action action = Action::None;
        int playerIndex = -1;
    };

    struct ShaderAttributes
    {
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> position;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> normal;
    };

    struct ShaderUniforms
    {
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> projectionMatrix;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> viewMatrix;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> modelMatrix;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> cellColor;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> cellGlow;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> lightDirection;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> lightColor;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> ambientStrength;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> glowColor;
    };

    struct FlatShaderAttributes
    {
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> position;
    };

    struct FlatShaderUniforms
    {
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> projectionMatrix;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> viewMatrix;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> modelMatrix;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> color;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> glowStrength;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> glowColor;
    };

    struct TextMesh
    {
        GLuint vbo = 0;
        GLuint ibo = 0;
        GLsizei indexCount = 0;
    };

    struct WaveformMesh
    {
        GLuint vbo = 0;
        GLsizei vertexCount = 0;
        size_t pointCount = 0;
    };

    struct Palette
    {
        juce::Colour background;
        juce::Colour cellIdle;
        juce::Colour cellSelected;
        juce::Colour cellAccent;
        juce::Colour cellDisabled;
        juce::Colour textPrimary;
        juce::Colour textMuted;
        juce::Colour glowActive;
        juce::Colour lightColor;
        float ambientStrength = 0.32f;
        juce::Vector3D<float> lightDirection { 0.2f, 0.45f, 1.0f };
    };

    SuperSamplerProcessor& processorRef;

    juce::OpenGLContext openGLContext;
    std::unique_ptr<juce::OpenGLShaderProgram> shaderProgram;
    std::unique_ptr<ShaderAttributes> shaderAttributes;
    std::unique_ptr<ShaderUniforms> shaderUniforms;
    std::unique_ptr<juce::OpenGLShaderProgram> flatShaderProgram;
    std::unique_ptr<FlatShaderAttributes> flatShaderAttributes;
    std::unique_ptr<FlatShaderUniforms> flatShaderUniforms;

    std::unordered_map<std::string, TextMesh> textMeshCache;
    std::unordered_map<int, WaveformMesh> waveformMeshes;
    std::unordered_set<int> dirtyWaveforms;

    Segment14Geometry::Params textGeomParams{};
    Segment14Geometry textGeometry{ textGeomParams };

    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    GLuint edgeIndexBuffer = 0;

    Palette palette{};

    std::vector<PlayerUIState> players;
    std::vector<std::vector<CellVisualState>> cellStates;
    std::vector<std::vector<std::string>> cellText;
    std::vector<std::vector<CellInfo>> cellInfo;

    std::mutex uiMutex;
    std::mutex stateMutex;
    juce::var pendingPayload;
    bool pendingPayloadReady = false;
    std::atomic<bool> textMeshesDirty { true };

    size_t cursorRow = 0;
    size_t cursorCol = 0;
    bool editMode = false;
    Action editAction = Action::None;
    int editPlayerIndex = -1;

    float zoomLevel = 1.0f;
    float panOffsetX = 0.0f;
    float panOffsetY = 0.0f;
    juce::Point<int> lastDragPosition;

    void refreshFromProcessor();
    void refreshFromPayload(const juce::var& payload);
    void rebuildCellLayout();
    void rebuildTextMeshes();
    void updateWaveformMesh(int playerId, const std::vector<float>& points);
    void adjustZoom(float delta);

    void handleAction(const CellInfo& info);
    void adjustEditValue(int direction);
    void moveCursor(int deltaRow, int deltaCol);

    juce::Matrix3D<float> getProjectionMatrix(float aspectRatio) const;
    juce::Matrix3D<float> getViewMatrix() const;
    juce::Matrix3D<float> getModelMatrix(juce::Vector3D<float> position, juce::Vector3D<float> scale) const;

    juce::Colour getCellColour(const CellVisualState& cell, const CellInfo& info) const;
    juce::Colour getTextColour(const CellVisualState& cell, const CellInfo& info) const;
    float getCellDepthScale(const CellVisualState& cell) const;

    TextMesh& ensureTextMesh(const std::string& text);
    void clearTextMeshes();
    void clearWaveformMeshes();
};
