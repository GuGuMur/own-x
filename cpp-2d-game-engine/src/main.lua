print(_VERSION)
local GL_COLOR_BUFFER_BIT<const> = 0x00004000

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
print(shader)
function draw()
--     print(winW, winH)
        glClearColor(0.5, 0.5, 0.5, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0, 0, winW, winH);
end