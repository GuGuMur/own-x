print(_VERSION)
local GL_COLOR_BUFFER_BIT<const> = 0x00004000
local GL_FLOAT<const> = 0x1406
local GL_POINTS<const> = 0x0000
local GL_LINES<const> = 0x0001
local GL_TRIANGLE_STRIP<const> = 0x0005
local GL_BLEND<const> = 0x0BE2
local GL_SRC_ALPHA<const> = 0x0302
local GL_ONE_MINUS_SRC_ALPHA<const> = 0x0303


audioOpen("data/MeetingTheStars.ogg", 0);
audioOpen("data/SadSoul.ogg");
-- audioPause(0);
local vsSrc<const> =
[[
    #version 330 core
    attribute vec2 position;
    void main() {
        float x = 2.0 * position.x / (640.0 - 1.0) - 1.0;
        float y = 1.0 - 2.0 * position.y / (480.0 - 1.0);
        gl_Position = vec4(x, y, 0.0, 1.0);
    }
]]

local fsSrc<const> =
[[
    #version 330 core
    uniform vec4 color;
    void main() {
        // gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
        gl_FragColor = color;
    }
]]

local shader = newShader(vsSrc, fsSrc)

local texture = newTexture("data/uvchecker.png");

local vsSrcUV = [[
    #version 330 core
    attribute vec2 position;
    attribute vec2 texcoord;
    varying vec2 uv;
    void main() {
        float x = 2.0 * position.x / (640.0 - 1.0) - 1.0;
        float y = 1.0 - 2.0 * position.y / (480.0 - 1.0);
        gl_Position = vec4(x, y, 0.0, 1.0);
        uv = texcoord;
    }
]]

local fsSrcUV = [[
    #version 330 core
    uniform vec4 color;
    uniform sampler2D texture0;
    varying vec2 uv;
    void main() {
        vec4 texcolor = texture2D(texture0, uv);
        gl_FragColor = color * texcolor;
    }
]]
local shaderUV = newShader(vsSrcUV, fsSrcUV)

local buffer = newBuffer(150, 50, 300, 300)

local bufferPoint = newBuffer({50, 100})
local bufferLine = newBuffer({0, 0, winW - 1, winH - 1})
local bufferRect = newBuffer(150, 50, 350, 350, false)

local font = newFont("data/AlibabaPuHuiTi-3-55-Regular.ttf");
local bitmap, x0, y0, w, h = font:makeBitmap(utf8.codepoint("啊"), 50);

local textureFont = newTexture(bitmap, w, h);

local bufferFont = newBuffer(150, 50, w, h)
local fsSrcFont = [[
    #version 330 core
    uniform vec4 color;
    uniform sampler2D texture0;
    varying vec2 uv;
    void main() {
        vec4 texcolor = texture2D(texture0, uv);
        gl_FragColor = vec4(color.rgb, color.a * texcolor.a);
    }
]]

local shaderfont = newShader(vsSrcUV, fsSrcFont);

local function drawPoint(buffer, shader)
        buffer:bind();
        shader:attrib("position", 2, GL_FLOAT);
        shader:use();
        shader:setVec4("color", 1.0, 0.0, 0.0, 1.0);
        glDrawArrays(GL_POINTS, 0, 2);
        buffer:unbind();
end
local function drawLine(buffer, shader)
        buffer:bind();
        shader:attrib("position", 2, GL_FLOAT);
        shader:use();
        shader:setVec4("color", 0.0, 1.0, 0.0, 1.0);
        glDrawArrays(GL_LINES, 0, 2);
        buffer:unbind();
end
local function drawRect(buffer, shader)
        buffer:bind();
        shader:attrib("position", 2, GL_FLOAT);
        shader:use();
        shader:setVec4("color", 0.0, 0.0, 1.0, 1.0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        buffer:unbind();
end

local function drawRectUV(buffer, shader, texture)
        buffer:bind();
        shader:attrib("position", 2, GL_FLOAT, 4 * 4, nullptr);
        shader:attrib("texcoord", 2, GL_FLOAT, 4 * 4, 2 * 4);
        shader:use();
        shader:setVec4("color", 1.0, 1.0, 1.0, 1.0);
        shader:setTexture("texture0", 0);
        texture:bind(0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        texture:unbind();
        buffer:unbind();
end

function draw()
--     print(winW, winH)
        glClearColor(0.5, 0.5, 0.5, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0, 0, winW, winH);
        drawPoint(bufferPoint, shader)
        drawLine(bufferLine, shader)
        drawRect(bufferRect, shader)
        drawRectUV(buffer, shaderUV, texture)

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawRectUV(bufferFont, shaderfont, textureFont);
        glDisable(GL_BLEND);

--         if MouseEvent ~= 0 then print(MouseEvent, MouseX, MouseY, MouseButton) end -- ~=为不等于
--         if KeyEvent ~= 0 then print(KeyEvent, KeyCode) end
end