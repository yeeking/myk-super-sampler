#include "SuperSamplerEditor.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <unordered_set>

using namespace juce::gl;

namespace
{
struct Vertex
{
    float position[3];
    float normal[3];
};

const char* vertexShaderSource = R"(
    attribute vec3 position;
    attribute vec3 normal;
    uniform mat4 projectionMatrix;
    uniform mat4 viewMatrix;
    uniform mat4 modelMatrix;
    varying vec3 vNormal;

    void main()
    {
        vec4 worldNormal = modelMatrix * vec4(normal, 0.0);
        vNormal = normalize(worldNormal.xyz);
        gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(position, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    varying vec3 vNormal;
    uniform vec4 cellColor;
    uniform float cellGlow;
    uniform vec3 lightDirection;
    uniform vec3 lightColor;
    uniform float ambientStrength;
    uniform vec3 glowColor;

    void main()
    {
        vec3 normal = normalize(vNormal);
        vec3 lightDir = normalize(lightDirection);
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 litColor = cellColor.rgb * (ambientStrength + diff * lightColor);
        vec3 glow = glowColor * cellGlow;
        gl_FragColor = vec4(litColor + glow, cellColor.a);
    }
)";

const char* flatVertexShaderSource = R"(
    attribute vec3 position;
    uniform mat4 projectionMatrix;
    uniform mat4 viewMatrix;
    uniform mat4 modelMatrix;

    void main()
    {
        gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(position, 1.0);
    }
)";

const char* flatFragmentShaderSource = R"(
    uniform vec4 color;
    uniform float glowStrength;
    uniform vec3 glowColor;

    void main()
    {
        vec3 base = color.rgb + glowColor * glowStrength;
        gl_FragColor = vec4(base, color.a);
    }
)";

const Vertex cubeVertices[] =
{
    { { -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } },
    { {  0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } },
    { {  0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } },
    { { -0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f } },
    { {  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f } },
    { {  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
    { {  0.5f,  0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
    { {  0.5f,  0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f } },
    { {  0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f } },
    { { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f } },
    { { -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f } },
    { {  0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f } },
    { { -0.5f, -0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f } },
    { { -0.5f, -0.5f,  0.5f }, { -1.0f, 0.0f, 0.0f } },
    { { -0.5f,  0.5f,  0.5f }, { -1.0f, 0.0f, 0.0f } },
    { { -0.5f,  0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f } },
    { { -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f } },
    { {  0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f } },
    { {  0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
    { { -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } },
    { { -0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f } },
    { {  0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f } },
    { {  0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f, 0.0f } },
    { { -0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f, 0.0f } }
};

const GLuint cubeIndices[] =
{
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4,
    8, 9, 10, 10, 11, 8,
    12, 13, 14, 14, 15, 12,
    16, 17, 18, 18, 19, 16,
    20, 21, 22, 22, 23, 20
};
const GLsizei cubeIndexCount = static_cast<GLsizei>(sizeof(cubeIndices) / sizeof(cubeIndices[0]));

const GLuint cubeEdgeIndices[] =
{
    0, 1, 1, 2, 2, 3, 3, 0,
    8, 9, 9, 10, 10, 11, 11, 8,
    0, 13, 1, 4, 2, 7, 3, 16
};
const GLsizei cubeEdgeIndexCount = static_cast<GLsizei>(sizeof(cubeEdgeIndices) / sizeof(cubeEdgeIndices[0]));

juce::Matrix3D<float> makeScaleMatrix(juce::Vector3D<float> scale)
{
    return { scale.x, 0.0f, 0.0f, 0.0f,
             0.0f, scale.y, 0.0f, 0.0f,
             0.0f, 0.0f, scale.z, 0.0f,
             0.0f, 0.0f, 0.0f, 1.0f };
}

std::string sanitizeLabel(const juce::String& input, size_t maxLen)
{
    auto trimmed = input.trim();
    if (trimmed.isEmpty())
        return {};

    auto upper = trimmed.toUpperCase();
    std::string out;
    out.reserve(static_cast<size_t>(upper.length()));
    for (auto ch : upper)
    {
        if (out.size() >= maxLen)
            break;
        if (ch == '\n' || ch == '\r')
            continue;
        if (ch < 32 || ch > 126)
            continue;
        out.push_back(static_cast<char>(ch));
    }
    return out;
}

std::string formatGain(float gain)
{
    juce::String text = juce::String(gain, 2);
    return sanitizeLabel(text, 6);
}
} // namespace

SuperSamplerEditor::SuperSamplerEditor (SuperSamplerProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p)
{
    palette.background = juce::Colour(0xFF03060B);
    palette.cellIdle = juce::Colour(0xFF141A22);
    palette.cellSelected = juce::Colour(0xFF00E8FF);
    palette.cellAccent = juce::Colour(0xFF1E2F3D);
    palette.cellDisabled = juce::Colour(0xFF0C1118);
    palette.textPrimary = juce::Colour(0xFF4EF2C2);
    palette.textMuted = juce::Colour(0xFF6B7C8F);
    palette.glowActive = juce::Colour(0xFFFF5533);
    palette.lightColor = juce::Colour(0xFFEAF6FF);

    textGeomParams.cellW = 1.0f;
    textGeomParams.cellH = 1.5f;
    textGeomParams.thickness = 0.14f;
    textGeomParams.inset = 0.1f;
    textGeomParams.gap = 0.06f;
    textGeomParams.advance = 1.05f;
    textGeomParams.includeDot = true;
    textGeometry.setParams(textGeomParams);

    openGLContext.setRenderer(this);
    openGLContext.setContinuousRepainting(false);
    // this will stop it from rendering the JUCE layer
    // set to true if you want to draw juce components into the opengl space
    openGLContext.setComponentPaintingEnabled(false);
    openGLContext.attachTo(*this);

    setSize (980, 640);

    addKeyListener(this);
    setWantsKeyboardFocus(true);

    startTimerHz(30);
    refreshFromProcessor();
}

SuperSamplerEditor::~SuperSamplerEditor()
{
    openGLContext.detach();
}

void SuperSamplerEditor::paint (juce::Graphics& g)
{
    g.fillAll (palette.background);
}

void SuperSamplerEditor::resized()
{
}

void SuperSamplerEditor::timerCallback()
{
    refreshFromProcessor();
    openGLContext.triggerRepaint();
}

void SuperSamplerEditor::updateUIFromProcessor(const juce::var& payload)
{
    const std::lock_guard<std::mutex> lock(stateMutex);
    pendingPayload = payload;
    pendingPayloadReady = true;
}

void SuperSamplerEditor::adjustZoom(float delta)
{
    const float minZoom = 0.5f;
    const float maxZoom = 2.5f;
    zoomLevel = juce::jlimit(minZoom, maxZoom, zoomLevel + delta);
}

void SuperSamplerEditor::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (!getLocalBounds().contains(event.getPosition()))
        return;

    const float zoomDelta = wheel.deltaY * 0.4f;
    if (std::abs(zoomDelta) < 0.0001f)
        return;

    const std::lock_guard<std::mutex> lock(uiMutex);
    adjustZoom(zoomDelta);
}

void SuperSamplerEditor::mouseDown (const juce::MouseEvent& event)
{
    if (!getLocalBounds().contains(event.getPosition()))
        return;

    lastDragPosition = event.getPosition();
}

void SuperSamplerEditor::mouseDrag (const juce::MouseEvent& event)
{
    if (!getLocalBounds().contains(event.getPosition()))
        return;

    const auto currentPos = event.getPosition();
    const auto delta = currentPos - lastDragPosition;
    lastDragPosition = currentPos;

    const std::lock_guard<std::mutex> lock(uiMutex);
    const float panScale = 0.02f / zoomLevel;
    panOffsetX += static_cast<float>(delta.x) * panScale;
    panOffsetY -= static_cast<float>(delta.y) * panScale;
}

void SuperSamplerEditor::refreshFromProcessor()
{
    juce::var payload;
    {
        const std::lock_guard<std::mutex> lock(stateMutex);
        if (pendingPayloadReady)
        {
            payload = pendingPayload;
            pendingPayloadReady = false;
        }
    }

    if (payload.isVoid())
        payload = processorRef.getSamplerState();

    refreshFromPayload(payload);
}

void SuperSamplerEditor::refreshFromPayload(const juce::var& payload)
{
    auto* obj = payload.getDynamicObject();
    if (obj == nullptr)
        return;

    auto playersVar = obj->getProperty("players");
    const auto* playersArray = playersVar.getArray();
    if (playersArray == nullptr)
        return;

    std::vector<PlayerUIState> nextPlayers;
    nextPlayers.reserve(static_cast<size_t>(playersArray->size()));

    std::vector<PlayerUIState> previousPlayers;
    {
        const std::lock_guard<std::mutex> lock(uiMutex);
        previousPlayers = players;
    }

    for (const auto& entry : *playersArray)
    {
        auto* playerObj = entry.getDynamicObject();
        if (playerObj == nullptr)
            continue;

        PlayerUIState st;
        st.id = static_cast<int>(playerObj->getProperty("id"));
        st.midiLow = static_cast<int>(playerObj->getProperty("midiLow"));
        st.midiHigh = static_cast<int>(playerObj->getProperty("midiHigh"));
        st.gain = static_cast<float>(double(playerObj->getProperty("gain")));
        st.isPlaying = static_cast<bool>(playerObj->getProperty("isPlaying"));
        st.status = playerObj->getProperty("status").toString();
        st.fileName = playerObj->getProperty("fileName").toString();
        st.filePath = playerObj->getProperty("filePath").toString();
        nextPlayers.push_back(st);
    }

    bool waveformChanged = false;
    std::vector<int> dirtyIds;
    for (auto& player : nextPlayers)
    {
        auto it = std::find_if(previousPlayers.begin(), previousPlayers.end(),
                               [&player](const PlayerUIState& existing) { return existing.id == player.id; });
        if (player.filePath.isEmpty())
        {
            player.waveformPoints.clear();
            continue;
        }

        bool needsWaveform = true;
        if (it != players.end())
        {
            needsWaveform = (player.filePath != it->filePath)
                || (player.status != it->status && player.status == "loaded")
                || (it->waveformPoints.empty() && player.status == "loaded");
        }

        if (needsWaveform)
        {
            player.waveformPoints = processorRef.getWaveformPointsForPlayer(player.id);
            waveformChanged = true;
            dirtyIds.push_back(player.id);
        }
        else if (it != players.end())
        {
            player.waveformPoints = it->waveformPoints;
        }
    }

    {
        const std::lock_guard<std::mutex> lock(uiMutex);
        for (int id : dirtyIds)
            dirtyWaveforms.insert(id);
        players = std::move(nextPlayers);
        rebuildCellLayout();
    }

    if (waveformChanged)
        textMeshesDirty.store(true);
}

void SuperSamplerEditor::rebuildCellLayout()
{
    const size_t rows = players.size() + 1;
    const size_t cols = 6;

    cursorRow = std::min(cursorRow, rows - 1);
    cursorCol = std::min(cursorCol, cols - 1);
    if (cursorRow == 0)
        cursorCol = 0;

    cellStates.assign(cols, std::vector<CellVisualState>(rows));
    cellText.assign(cols, std::vector<std::string>(rows));
    cellInfo.assign(cols, std::vector<CellInfo>(rows));

    for (size_t col = 0; col < cols; ++col)
    {
        for (size_t row = 0; row < rows; ++row)
        {
            CellInfo info{};
            if (row == 0)
            {
                if (col == 0)
                {
                    info.action = Action::Add;
                    info.playerIndex = -1;
                    cellText[col][row] = "ADD";
                }
                else
                {
                    info.action = Action::None;
                    cellText[col][row].clear();
                }
            }
            else
            {
                const auto& player = players[row - 1];
                info.playerIndex = static_cast<int>(row - 1);
                switch (col)
                {
                    case 0:
                        info.action = Action::Load;
                        cellText[col][row] = "LOAD";
                        break;
                    case 1:
                        info.action = Action::Trigger;
                        cellText[col][row] = player.isPlaying ? "PLAY" : "TRIG";
                        break;
                    case 2:
                        info.action = Action::Low;
                        cellText[col][row] = sanitizeLabel(juce::String(player.midiLow), 4);
                        break;
                    case 3:
                        info.action = Action::High;
                        cellText[col][row] = sanitizeLabel(juce::String(player.midiHigh), 4);
                        break;
                    case 4:
                        info.action = Action::Gain;
                        cellText[col][row] = formatGain(player.gain);
                        break;
                    case 5:
                        info.action = Action::Waveform;
                        cellText[col][row] = sanitizeLabel(player.fileName.isNotEmpty() ? player.fileName : player.status, 18);
                        break;
                    default:
                        info.action = Action::None;
                        break;
                }
            }

            cellInfo[col][row] = info;
            auto& visual = cellStates[col][row];
            visual.isSelected = (row == cursorRow && col == cursorCol);
            visual.isEditing = (editMode && visual.isSelected);
            visual.isActive = (info.action == Action::Trigger && row > 0 && players[row - 1].isPlaying);
            visual.isDisabled = (info.action == Action::None);
            visual.glow = visual.isActive ? 1.0f : 0.0f;
        }
    }

    textMeshesDirty.store(true);
}

void SuperSamplerEditor::rebuildTextMeshes()
{
    std::unordered_set<std::string> needed;
    for (const auto& col : cellText)
        for (const auto& text : col)
            if (!text.empty())
                needed.insert(text);

    std::vector<std::string> toRemove;
    for (const auto& entry : textMeshCache)
        if (needed.find(entry.first) == needed.end())
            toRemove.push_back(entry.first);

    for (const auto& key : toRemove)
    {
        auto it = textMeshCache.find(key);
        if (it != textMeshCache.end())
        {
            if (it->second.vbo != 0)
                openGLContext.extensions.glDeleteBuffers(1, &it->second.vbo);
            if (it->second.ibo != 0)
                openGLContext.extensions.glDeleteBuffers(1, &it->second.ibo);
            textMeshCache.erase(it);
        }
    }

    for (const auto& text : needed)
        ensureTextMesh(text);
}

void SuperSamplerEditor::newOpenGLContextCreated()
{
    shaderProgram = std::make_unique<juce::OpenGLShaderProgram>(openGLContext);
    if (shaderProgram->addVertexShader(juce::OpenGLHelpers::translateVertexShaderToV3(vertexShaderSource))
        && shaderProgram->addFragmentShader(juce::OpenGLHelpers::translateFragmentShaderToV3(fragmentShaderSource))
        && shaderProgram->link())
    {
        shaderAttributes = std::make_unique<ShaderAttributes>();
        shaderAttributes->position = std::make_unique<juce::OpenGLShaderProgram::Attribute>(*shaderProgram, "position");
        shaderAttributes->normal = std::make_unique<juce::OpenGLShaderProgram::Attribute>(*shaderProgram, "normal");

        shaderUniforms = std::make_unique<ShaderUniforms>();
        shaderUniforms->projectionMatrix = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*shaderProgram, "projectionMatrix");
        shaderUniforms->viewMatrix = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*shaderProgram, "viewMatrix");
        shaderUniforms->modelMatrix = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*shaderProgram, "modelMatrix");
        shaderUniforms->cellColor = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*shaderProgram, "cellColor");
        shaderUniforms->cellGlow = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*shaderProgram, "cellGlow");
        shaderUniforms->lightDirection = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*shaderProgram, "lightDirection");
        shaderUniforms->lightColor = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*shaderProgram, "lightColor");
        shaderUniforms->ambientStrength = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*shaderProgram, "ambientStrength");
        shaderUniforms->glowColor = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*shaderProgram, "glowColor");
    }

    flatShaderProgram = std::make_unique<juce::OpenGLShaderProgram>(openGLContext);
    if (flatShaderProgram->addVertexShader(juce::OpenGLHelpers::translateVertexShaderToV3(flatVertexShaderSource))
        && flatShaderProgram->addFragmentShader(juce::OpenGLHelpers::translateFragmentShaderToV3(flatFragmentShaderSource))
        && flatShaderProgram->link())
    {
        flatShaderAttributes = std::make_unique<FlatShaderAttributes>();
        flatShaderAttributes->position = std::make_unique<juce::OpenGLShaderProgram::Attribute>(*flatShaderProgram, "position");

        flatShaderUniforms = std::make_unique<FlatShaderUniforms>();
        flatShaderUniforms->projectionMatrix = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*flatShaderProgram, "projectionMatrix");
        flatShaderUniforms->viewMatrix = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*flatShaderProgram, "viewMatrix");
        flatShaderUniforms->modelMatrix = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*flatShaderProgram, "modelMatrix");
        flatShaderUniforms->color = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*flatShaderProgram, "color");
        flatShaderUniforms->glowStrength = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*flatShaderProgram, "glowStrength");
        flatShaderUniforms->glowColor = std::make_unique<juce::OpenGLShaderProgram::Uniform>(*flatShaderProgram, "glowColor");
    }

    openGLContext.extensions.glGenBuffers(1, &vertexBuffer);
    openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    openGLContext.extensions.glBufferData(GL_ARRAY_BUFFER,
                                          static_cast<GLsizeiptr>(sizeof(cubeVertices)),
                                          cubeVertices,
                                          GL_STATIC_DRAW);

    openGLContext.extensions.glGenBuffers(1, &indexBuffer);
    openGLContext.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    openGLContext.extensions.glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                          static_cast<GLsizeiptr>(sizeof(cubeIndices)),
                                          cubeIndices,
                                          GL_STATIC_DRAW);

    openGLContext.extensions.glGenBuffers(1, &edgeIndexBuffer);
    openGLContext.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edgeIndexBuffer);
    openGLContext.extensions.glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                          static_cast<GLsizeiptr>(sizeof(cubeEdgeIndices)),
                                          cubeEdgeIndices,
                                          GL_STATIC_DRAW);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    textMeshesDirty.store(true);
}

void SuperSamplerEditor::renderOpenGL()
{
    const std::lock_guard<std::mutex> lock(uiMutex);

    const float renderingScale = openGLContext.getRenderingScale();
    glViewport(0, 0, juce::roundToInt(renderingScale * (float)getWidth()),
               juce::roundToInt(renderingScale * (float)getHeight()));

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, juce::roundToInt(renderingScale * (float)getWidth()),
              juce::roundToInt(renderingScale * (float)getHeight()));

    const auto background = palette.background;
    glClearColor(background.getFloatRed(), background.getFloatGreen(), background.getFloatBlue(), 1.0f);
    // glClearColor(0.5f,0.5f,0.5f, 1.0f);
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    const float aspectRatio = getWidth() > 0 ? (float)getWidth() / (float)getHeight() : 1.0f;
    const auto projectionMatrix = getProjectionMatrix(aspectRatio);
    const auto viewMatrix = getViewMatrix();

    if (cellStates.empty() || cellStates[0].empty())
    {
        openGLContext.extensions.glUseProgram(0);
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    const float cellHeight = 1.1f;
    const float cellDepth = 0.7f;
    const float rowGap = 0.35f;
    const float colGap = 0.28f;

    std::vector<float> colWidths = { 1.2f, 1.2f, 1.1f, 1.1f, 1.4f, 5.0f };

    const float gridWidth = std::accumulate(colWidths.begin(), colWidths.end(), 0.0f)
                            + colGap * (colWidths.size() - 1);
    const float gridHeight = (cellHeight * (float)cellStates[0].size())
                             + rowGap * (float)(cellStates[0].size() - 1);

    const float startX = -gridWidth * 0.5f;
    const float startY = gridHeight * 0.5f;

    if (textMeshesDirty.exchange(false))
        rebuildTextMeshes();

    if (shaderProgram)
    {
        openGLContext.extensions.glUseProgram(shaderProgram->getProgramID());
        if (shaderUniforms->projectionMatrix)
            shaderUniforms->projectionMatrix->setMatrix4(projectionMatrix.mat, 1, GL_FALSE);
        if (shaderUniforms->viewMatrix)
            shaderUniforms->viewMatrix->setMatrix4(viewMatrix.mat, 1, GL_FALSE);
        if (shaderUniforms->lightDirection)
            shaderUniforms->lightDirection->set(palette.lightDirection.x, palette.lightDirection.y, palette.lightDirection.z);
        if (shaderUniforms->lightColor)
            shaderUniforms->lightColor->set(palette.lightColor.getFloatRed(),
                                            palette.lightColor.getFloatGreen(),
                                            palette.lightColor.getFloatBlue());
        if (shaderUniforms->ambientStrength)
            shaderUniforms->ambientStrength->set(palette.ambientStrength);
        if (shaderUniforms->glowColor)
            shaderUniforms->glowColor->set(palette.glowActive.getFloatRed(),
                                           palette.glowActive.getFloatGreen(),
                                           palette.glowActive.getFloatBlue());

        openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        openGLContext.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);

        const GLsizei stride = sizeof(Vertex);
        if (shaderAttributes->position)
        {
            openGLContext.extensions.glVertexAttribPointer(shaderAttributes->position->attributeID,
                                                           3,
                                                           GL_FLOAT,
                                                           GL_FALSE,
                                                           stride,
                                                           reinterpret_cast<GLvoid*>(static_cast<uintptr_t>(offsetof(Vertex, position))));
            openGLContext.extensions.glEnableVertexAttribArray(shaderAttributes->position->attributeID);
        }
        if (shaderAttributes->normal)
        {
            openGLContext.extensions.glVertexAttribPointer(shaderAttributes->normal->attributeID,
                                                           3,
                                                           GL_FLOAT,
                                                           GL_FALSE,
                                                           stride,
                                                           reinterpret_cast<GLvoid*>(static_cast<uintptr_t>(offsetof(Vertex, normal))));
            openGLContext.extensions.glEnableVertexAttribArray(shaderAttributes->normal->attributeID);
        }

        for (size_t row = 0; row < cellStates[0].size(); ++row)
        {
            float cursorX = startX;
            const float centerY = startY - (float)row * (cellHeight + rowGap) - cellHeight * 0.5f;

            for (size_t col = 0; col < colWidths.size(); ++col)
            {
                const float width = colWidths[col];
                const float centerX = cursorX + width * 0.5f;
                cursorX += width + colGap;

                const auto& cell = cellStates[col][row];
                const auto& info = cellInfo[col][row];
                const float depthScale = getCellDepthScale(cell);

                const auto position = juce::Vector3D<float>(centerX, centerY, 0.0f);
                const auto scale = juce::Vector3D<float>(width, cellHeight, cellDepth * depthScale);
                const auto modelMatrix = getModelMatrix(position, scale);

                const auto color = getCellColour(cell, info);
                if (shaderUniforms->cellColor)
                    shaderUniforms->cellColor->set(color.getFloatRed(),
                                                   color.getFloatGreen(),
                                                   color.getFloatBlue(),
                                                   color.getFloatAlpha());
                if (shaderUniforms->cellGlow)
                    shaderUniforms->cellGlow->set(cell.glow);
                if (shaderUniforms->modelMatrix)
                    shaderUniforms->modelMatrix->setMatrix4(modelMatrix.mat, 1, GL_FALSE);

                glDrawElements(GL_TRIANGLES, cubeIndexCount, GL_UNSIGNED_INT, nullptr);

                if (cell.isSelected)
                {
                    glDepthMask(GL_FALSE);
                    glLineWidth(1.8f);
                    openGLContext.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, edgeIndexBuffer);
                    if (shaderUniforms->cellColor)
                        shaderUniforms->cellColor->set(palette.cellSelected.getFloatRed(),
                                                       palette.cellSelected.getFloatGreen(),
                                                       palette.cellSelected.getFloatBlue(),
                                                       0.8f);
                    if (shaderUniforms->cellGlow)
                        shaderUniforms->cellGlow->set(0.2f);
                    glDrawElements(GL_LINES, cubeEdgeIndexCount, GL_UNSIGNED_INT, nullptr);
                    openGLContext.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
                    glDepthMask(GL_TRUE);
                }
            }
        }

        if (shaderAttributes->position)
            openGLContext.extensions.glDisableVertexAttribArray(shaderAttributes->position->attributeID);
        if (shaderAttributes->normal)
            openGLContext.extensions.glDisableVertexAttribArray(shaderAttributes->normal->attributeID);
    }

    if (flatShaderProgram)
    {
        openGLContext.extensions.glUseProgram(flatShaderProgram->getProgramID());
        if (flatShaderUniforms->projectionMatrix)
            flatShaderUniforms->projectionMatrix->setMatrix4(projectionMatrix.mat, 1, GL_FALSE);
        if (flatShaderUniforms->viewMatrix)
            flatShaderUniforms->viewMatrix->setMatrix4(viewMatrix.mat, 1, GL_FALSE);
        if (flatShaderAttributes->position)
            openGLContext.extensions.glEnableVertexAttribArray(flatShaderAttributes->position->attributeID);
        // draw box contents 
        for (size_t row = 0; row < cellStates[0].size(); ++row)
        {
            float cursorX = startX;
            const float centerY = startY - (float)row * (cellHeight + rowGap) - cellHeight * 0.5f;

            for (size_t col = 0; col < colWidths.size(); ++col)
            {
                const float width = colWidths[col];
                const float centerX = cursorX + width * 0.5f;
                cursorX += width + colGap;

                const auto& text = cellText[col][row];
                if (text.empty())
                    continue;

                auto meshIt = textMeshCache.find(text);
                if (meshIt == textMeshCache.end() || meshIt->second.indexCount == 0)
                    continue;

                const float targetHeight = cellHeight * 0.55f;
                const float targetWidth = width * 0.88f;
                const float textWidth = textGeomParams.advance * static_cast<float>(text.size());
                const float widthScale = textWidth > 0.0f ? targetWidth / textWidth : 1.0f;
                const float heightScale = targetHeight / textGeomParams.cellH;
                const float scale = std::min(widthScale, heightScale);
                const float textHeightScaled = textGeomParams.cellH * scale;

                const float baseX = centerX - (textWidth * scale) * 0.5f;
                const float baseY = centerY - textHeightScaled * 0.5f;
                const float zOffset = cellDepth * 0.55f;

                const auto position = juce::Vector3D<float>(baseX, baseY, zOffset);
                const auto scaleVec = juce::Vector3D<float>(scale, scale, 1.0f);
                const auto modelMatrix = getModelMatrix(position, scaleVec);

                const auto& cell = cellStates[col][row];
                const auto& info = cellInfo[col][row];
                const auto textColor = getTextColour(cell, info);

                if (flatShaderUniforms->color)
                    flatShaderUniforms->color->set(textColor.getFloatRed(),
                                                   textColor.getFloatGreen(),
                                                   textColor.getFloatBlue(),
                                                   textColor.getFloatAlpha());
                if (flatShaderUniforms->glowStrength)
                    flatShaderUniforms->glowStrength->set(cell.isSelected ? 0.2f : 0.0f);
                if (flatShaderUniforms->glowColor)
                    flatShaderUniforms->glowColor->set(palette.cellSelected.getFloatRed(),
                                                       palette.cellSelected.getFloatGreen(),
                                                       palette.cellSelected.getFloatBlue());
                if (flatShaderUniforms->modelMatrix)
                    flatShaderUniforms->modelMatrix->setMatrix4(modelMatrix.mat, 1, GL_FALSE);

                openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, meshIt->second.vbo);
                openGLContext.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshIt->second.ibo);
                if (flatShaderAttributes->position)
                    openGLContext.extensions.glVertexAttribPointer(flatShaderAttributes->position->attributeID,
                                                                   3,
                                                                   GL_FLOAT,
                                                                   GL_FALSE,
                                                                   sizeof(Segment14Geometry::Vertex),
                                                                           reinterpret_cast<GLvoid*>(static_cast<uintptr_t>(offsetof(Segment14Geometry::Vertex, x))));
                glDrawElements(GL_TRIANGLES, meshIt->second.indexCount, GL_UNSIGNED_INT, nullptr);
            }
        }// end of main drawing block for box contents

        
        for (size_t row = 1; row < cellStates[0].size(); ++row)
        {
            const auto& player = players[row - 1];
            if (player.waveformPoints.empty())
                continue;

            const size_t waveformCol = 5;
            float cursorX = startX;
            for (size_t col = 0; col < waveformCol; ++col)
                cursorX += colWidths[col] + colGap;

            const float width = colWidths[waveformCol];
            const float centerX = cursorX + width * 0.5f;
            const float centerY = startY - (float)row * (cellHeight + rowGap) - cellHeight * 0.5f;

            if (dirtyWaveforms.find(player.id) != dirtyWaveforms.end())
            {
                updateWaveformMesh(player.id, player.waveformPoints);
                dirtyWaveforms.erase(player.id);
            }

            auto meshIt = waveformMeshes.find(player.id);
            if (meshIt == waveformMeshes.end() || meshIt->second.vertexCount == 0)
                continue;

            const float waveHeight = cellHeight * 0.7f;
            const float waveWidth = width * 0.88f;
            const float zOffset = cellDepth * 0.6f;

            const auto position = juce::Vector3D<float>(centerX, centerY, zOffset);
            const auto scaleVec = juce::Vector3D<float>(waveWidth, waveHeight, 1.0f);
            const auto modelMatrix = getModelMatrix(position, scaleVec);

            if (flatShaderUniforms->color)
                flatShaderUniforms->color->set(palette.textMuted.getFloatRed(),
                                               palette.textMuted.getFloatGreen(),
                                               palette.textMuted.getFloatBlue(),
                                               0.9f);
            if (flatShaderUniforms->glowStrength)
                flatShaderUniforms->glowStrength->set(player.isPlaying ? 0.3f : 0.0f);
            if (flatShaderUniforms->glowColor)
                flatShaderUniforms->glowColor->set(palette.glowActive.getFloatRed(),
                                                   palette.glowActive.getFloatGreen(),
                                                   palette.glowActive.getFloatBlue());
            if (flatShaderUniforms->modelMatrix)
                flatShaderUniforms->modelMatrix->setMatrix4(modelMatrix.mat, 1, GL_FALSE);

            openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, meshIt->second.vbo);
            if (flatShaderAttributes->position)
                openGLContext.extensions.glVertexAttribPointer(flatShaderAttributes->position->attributeID,
                                                               3,
                                                               GL_FLOAT,
                                                               GL_FALSE,
                                                               sizeof(float) * 3,
                                                               nullptr);

            glLineWidth(1.4f);
            glDrawArrays(GL_LINE_STRIP, GLint{0}, meshIt->second.vertexCount);
        }
        

        if (flatShaderAttributes->position)
            openGLContext.extensions.glDisableVertexAttribArray(flatShaderAttributes->position->attributeID);
    }

    openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, GLuint{0});
    openGLContext.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GLuint{0});
    openGLContext.extensions.glUseProgram(GLuint{0});
    glDisable(GL_SCISSOR_TEST);
    // glDisable(GL_CULL_FACE);
    // glDisable(GL_DEPTH_TEST);
}

void SuperSamplerEditor::openGLContextClosing()
{
    shaderAttributes.reset();
    shaderUniforms.reset();
    shaderProgram.reset();
    flatShaderAttributes.reset();
    flatShaderUniforms.reset();
    flatShaderProgram.reset();

    clearTextMeshes();
    clearWaveformMeshes();

    if (vertexBuffer != 0)
    {
        openGLContext.extensions.glDeleteBuffers(1, &vertexBuffer);
        vertexBuffer = 0;
    }

    if (indexBuffer != 0)
    {
        openGLContext.extensions.glDeleteBuffers(1, &indexBuffer);
        indexBuffer = 0;
    }

    if (edgeIndexBuffer != 0)
    {
        openGLContext.extensions.glDeleteBuffers(1, &edgeIndexBuffer);
        edgeIndexBuffer = 0;
    }
}

bool SuperSamplerEditor::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (editMode)
    {
        if (key == juce::KeyPress::escapeKey || key == juce::KeyPress::returnKey)
        {
            editMode = false;
            editAction = Action::None;
            editPlayerIndex = -1;
            return true;
        }

        if (key.getKeyCode() == juce::KeyPress::leftKey || key.getKeyCode() == juce::KeyPress::downKey)
        {
            adjustEditValue(-1);
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::rightKey || key.getKeyCode() == juce::KeyPress::upKey)
        {
            adjustEditValue(1);
            return true;
        }
        return false;
    }

    if (key.getKeyCode() == juce::KeyPress::leftKey)
    {
        moveCursor(0, -1);
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::rightKey)
    {
        moveCursor(0, 1);
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::upKey)
    {
        moveCursor(-1, 0);
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::downKey)
    {
        moveCursor(1, 0);
        return true;
    }

    if (key == juce::KeyPress::returnKey || key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (cursorCol < cellInfo.size() && cursorRow < cellInfo[cursorCol].size())
            handleAction(cellInfo[cursorCol][cursorRow]);
        return true;
    }

    return false;
}

void SuperSamplerEditor::handleAction(const CellInfo& info)
{
    if (info.action == Action::None)
        return;

    if (info.action == Action::Add)
    {
        processorRef.addSamplePlayerFromWeb();
        return;
    }

    int playerId = -1;
    {
        const std::lock_guard<std::mutex> lock(uiMutex);
        if (info.playerIndex < 0 || info.playerIndex >= static_cast<int>(players.size()))
            return;
        playerId = players[(size_t)info.playerIndex].id;
    }

    switch (info.action)
    {
        case Action::None:
        case Action::Add:
        case Action::Waveform:
            break;
        case Action::Load:
            processorRef.requestSampleLoadFromWeb(playerId);
            break;
        case Action::Trigger:
            processorRef.triggerFromWeb(playerId);
            break;
        case Action::Low:
        case Action::High:
        case Action::Gain:
            editMode = true;
            editAction = info.action;
            editPlayerIndex = info.playerIndex;
            break;
        default:
            break;
    }
}

void SuperSamplerEditor::adjustEditValue(int direction)
{
    PlayerUIState player;
    {
        const std::lock_guard<std::mutex> lock(uiMutex);
        if (editPlayerIndex < 0 || editPlayerIndex >= static_cast<int>(players.size()))
            return;
        player = players[(size_t)editPlayerIndex];
    }
    if (editAction == Action::Low)
    {
        const int low = juce::jlimit(0, 127, player.midiLow + direction);
        processorRef.setSampleRangeFromWeb(player.id, low, player.midiHigh);
    }
    else if (editAction == Action::High)
    {
        const int high = juce::jlimit(0, 127, player.midiHigh + direction);
        processorRef.setSampleRangeFromWeb(player.id, player.midiLow, high);
    }
    else if (editAction == Action::Gain)
    {
        const float gain = juce::jlimit(0.0f, 2.0f, player.gain + direction * 0.05f);
        processorRef.setGainFromUI(player.id, gain);
    }
}

void SuperSamplerEditor::moveCursor(int deltaRow, int deltaCol)
{
    const std::lock_guard<std::mutex> lock(uiMutex);

    if (cellStates.empty() || cellStates[0].empty())
        return;

    const int maxRow = static_cast<int>(cellStates[0].size()) - 1;
    const int maxCol = static_cast<int>(cellStates.size()) - 1;

    int nextRow = juce::jlimit(0, maxRow, static_cast<int>(cursorRow) + deltaRow);
    int nextCol = juce::jlimit(0, maxCol, static_cast<int>(cursorCol) + deltaCol);

    if (nextRow == 0)
        nextCol = 0;

    cursorRow = static_cast<size_t>(nextRow);
    cursorCol = static_cast<size_t>(nextCol);
    rebuildCellLayout();
}

void SuperSamplerEditor::updateWaveformMesh(int playerId, const std::vector<float>& points)
{
    if (points.size() < 2)
        return;

    const size_t pointCount = points.size() / 2;
    if (pointCount < 2)
        return;

    std::vector<float> vertices;
    vertices.reserve(pointCount * 3);

    const float width = 1.0f;
    const float height = 1.0f;
    const float halfW = width * 0.5f;
    const float halfH = height * 0.5f;

    for (size_t i = 0; i < pointCount; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(pointCount - 1);
        const float x = -halfW + t * width;
        const float yMax = juce::jlimit(-1.0f, 1.0f, points[i * 2 + 1]) * halfH;
        vertices.push_back(x);
        vertices.push_back(yMax);
        vertices.push_back(0.0f);
    }

    auto& mesh = waveformMeshes[playerId];
    mesh.vertexCount = static_cast<GLsizei>(vertices.size() / 3);
    mesh.pointCount = pointCount;

    if (mesh.vbo == 0)
        openGLContext.extensions.glGenBuffers(1, &mesh.vbo);

    openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    openGLContext.extensions.glBufferData(GL_ARRAY_BUFFER,
                                          static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                                          vertices.data(),
                                          GL_STATIC_DRAW);
}

SuperSamplerEditor::TextMesh& SuperSamplerEditor::ensureTextMesh(const std::string& text)
{
    auto& entry = textMeshCache[text];
    if (entry.indexCount > 0)
        return entry;

    const auto mesh = textGeometry.buildStringMesh(text);
    if (mesh.vertices.empty() || mesh.indices.empty())
        return entry;

    openGLContext.extensions.glGenBuffers(1, &entry.vbo);
    openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, entry.vbo);
    openGLContext.extensions.glBufferData(GL_ARRAY_BUFFER,
                                          static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Segment14Geometry::Vertex)),
                                          mesh.vertices.data(),
                                          GL_STATIC_DRAW);

    openGLContext.extensions.glGenBuffers(1, &entry.ibo);
    openGLContext.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry.ibo);
    openGLContext.extensions.glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                          static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint32_t)),
                                          mesh.indices.data(),
                                          GL_STATIC_DRAW);

    entry.indexCount = static_cast<GLsizei>(mesh.indices.size());
    return entry;
}

void SuperSamplerEditor::clearTextMeshes()
{
    for (auto& entry : textMeshCache)
    {
        if (entry.second.vbo != 0)
            openGLContext.extensions.glDeleteBuffers(1, &entry.second.vbo);
        if (entry.second.ibo != 0)
            openGLContext.extensions.glDeleteBuffers(1, &entry.second.ibo);
    }
    textMeshCache.clear();
}

void SuperSamplerEditor::clearWaveformMeshes()
{
    for (auto& entry : waveformMeshes)
    {
        if (entry.second.vbo != 0)
            openGLContext.extensions.glDeleteBuffers(1, &entry.second.vbo);
    }
    waveformMeshes.clear();
}

juce::Matrix3D<float> SuperSamplerEditor::getProjectionMatrix(float aspectRatio) const
{
    const float nearPlane = 6.0f;
    const float farPlane = 120.0f;
    const float frustumHeight = 3.4f;
    const float frustumWidth = frustumHeight * aspectRatio;

    return juce::Matrix3D<float>::fromFrustum(-frustumWidth, frustumWidth,
                                              -frustumHeight, frustumHeight,
                                              nearPlane, farPlane);
}

juce::Matrix3D<float> SuperSamplerEditor::getViewMatrix() const
{
    const float baseDistance = 22.0f;
    const float cameraDistance = baseDistance / zoomLevel;
    return juce::Matrix3D<float>::fromTranslation({panOffsetX, panOffsetY, -cameraDistance});
}

juce::Matrix3D<float> SuperSamplerEditor::getModelMatrix(juce::Vector3D<float> position, juce::Vector3D<float> scale) const
{
    return juce::Matrix3D<float>::fromTranslation(position) * makeScaleMatrix(scale);
}

juce::Colour SuperSamplerEditor::getCellColour(const CellVisualState& cell, const CellInfo& info) const
{
    if (cell.isDisabled)
        return palette.cellDisabled;
    if (cell.isSelected)
        return palette.cellSelected;
    if (cell.isActive)
        return palette.cellAccent;
    if (info.action == Action::Waveform)
        return palette.cellIdle.brighter(0.2f);
    return palette.cellIdle;
}

juce::Colour SuperSamplerEditor::getTextColour(const CellVisualState& cell, const CellInfo& info) const
{
    if (cell.isSelected)
        return palette.cellSelected;
    if (cell.isActive)
        return palette.glowActive;
    if (info.action == Action::Waveform)
        return palette.textMuted;
    return palette.textPrimary;
}

float SuperSamplerEditor::getCellDepthScale(const CellVisualState& cell) const
{
    if (cell.isEditing)
        return 1.05f;
    if (cell.isSelected)
        return 1.02f;
    return 1.0f;
}
