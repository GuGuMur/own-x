#include <SDL.h>
#include <cstdio>
#include <string>
#include <glad/glad.h>
#include <vector>
#include <stb/stb_image.h>
#include <stb/stb_truetype.h>
// #define STB_VORBIS_HEADER_ONLY
#include <stb/stb_vorbis.c>
#include <miniz.h>
#undef L // conflict between lua and stb_vorbis
#undef R
extern "C" {
#include <lua5.4/lua.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>
}

static int winW = 640;
static int winH = 480;

namespace {
    class Zip {
        mz_zip_archive zip{};

    public:
        Zip(const char *name) {
            memset(&zip, 0, sizeof(zip));
            mz_bool ret = mz_zip_reader_init_file(&zip, name, 0);
            if (ret == MZ_FALSE) {
                printf("file not found: %s\n", name);
                exit(-1);
            }
        }

        ~Zip() {
            mz_zip_reader_end(&zip);
        }

        void *open(const char *name, size_t &size) {
            return mz_zip_reader_extract_file_to_heap(&zip, name, &size, 0);
        }

        void close(void *p) {
            mz_free(p);
        }
    };

    Zip *gZip;

    class Shader {
        GLuint vsID;
        GLuint fsID;
        GLuint programID;

    public:
        Shader(const char *vsSrc, const char *fsSrc) {
            auto checkShaderInfo = [](const GLuint id) {
                GLint len;
                glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
                if (len != 0) {
                    std::vector<char> info(len + 1);
                    glGetShaderInfoLog(id, len, nullptr, info.data());
                    info[len] = *"\0";
                    printf("%s\n", info.data());
                }
            };
            auto checkProgramInfo = [](const GLuint id) {
                GLint len;
                glGetProgramiv(id, GL_INFO_LOG_LENGTH, &len);
                if (len != 0) {
                    std::vector<char> info(len + 1);
                    glGetProgramInfoLog(id, len, nullptr, info.data());
                    info[len] = *"\0";
                    printf("%s\n", info.data());
                }
            };
            vsID = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vsID, 1, &vsSrc, nullptr);
            glCompileShader(vsID);
            checkShaderInfo(vsID);

            fsID = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fsID, 1, &fsSrc, nullptr);
            glCompileShader(fsID);
            checkShaderInfo(fsID);

            programID = glCreateProgram();
            glAttachShader(programID, vsID);
            glAttachShader(programID, fsID);
            glLinkProgram(programID);
            checkProgramInfo(programID);
        }

        [[nodiscard]] GLuint getID() const { return programID; }

        void attrib(const char *name, const GLint size, const GLenum type, const GLsizei stride = 0,
                    const void *pointer = nullptr) const {
            const GLint location = glGetAttribLocation(programID, name);
            glEnableVertexAttribArray(location);
            glVertexAttribPointer(location, size, type, GL_FALSE, stride, pointer);
        }

        void setVec4(const char *name, const float v0, const float v1, const float v2, const float v3) const {
            // x, y, z, w
            const GLint location = glGetUniformLocation(programID, name);
            glUniform4f(location, v0, v1, v2, v3);
        }

        void setTexture(const char *name, const GLint texture) const {
            const GLint location = glGetUniformLocation(programID, name);
            glUniform1i(location, texture);
        }

        void use() const {
            glUseProgram(programID);
        }

        ~Shader() {
            glDeleteShader(vsID);
            glDeleteShader(fsID);
            glDeleteProgram(programID);
        }
    };

    class Buffer {
        GLuint bufferID{};

        void makeBuffer(const std::vector<float> &point) {
            glGenBuffers(1, &bufferID);
            glBindBuffer(GL_ARRAY_BUFFER, bufferID);
            glBufferData(GL_ARRAY_BUFFER, point.size() * sizeof(float), point.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        };

    public:
        Buffer(float x, float y, float w, float h, bool uv = true) {
            // uv: use texture
            const float l = x;
            const float r = x + w;
            const float t = y;
            const float b = y + h;
            const std::vector<float> rect = {
                l, t,
                l, b,
                r, t,
                r, b,
            };
            const std::vector<float> rectUV = {
                l, t, 0, 0,
                l, b, 0, 1,
                r, t, 1, 0,
                r, b, 1, 1,
            };
            makeBuffer(uv ? rectUV : rect);
        }

        explicit Buffer(const std::vector<float> &point) {
            makeBuffer(point);
        }

        [[nodiscard]] GLuint getID() const { return bufferID; }

        void bind() const {
            glBindBuffer(GL_ARRAY_BUFFER, bufferID);
        }

        static void unbind() {
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        ~Buffer() {
            glDeleteBuffers(1, &bufferID);
        }
    };

    class Texture {
        GLuint textureID;

    public:
        Texture(const char *name) {
            int w, h, c;
            stbi_uc *p = stbi_load(name, &w, &h, &c, 4); // r g b a 四个通道
            if (p == nullptr) {
                size_t size;
                void *data = gZip->open(name, size);
                if (data == nullptr) {
                    printf("[ERROR] failed to load %s\n", name);
                    exit(-1);
                } else {
                    p = stbi_load_from_memory(reinterpret_cast<stbi_uc *>(data), static_cast<int>(size), &w, &h, &c, 4);
                    gZip->close(data);
                }
            }
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            stbi_image_free(p);
        }

        Texture(const std::vector<unsigned char> &bitmap, int w, int h) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, bitmap.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        [[nodiscard]] GLuint getID() const { return textureID; }

        void bind(GLint texture) const {
            glActiveTexture(GL_TEXTURE0 + texture);
            glBindTexture(GL_TEXTURE_2D, textureID);
        }

        void unbind() const {
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        ~Texture() {
            glDeleteTextures(1, &textureID);
        }
    };

    class Font {
        std::vector<unsigned char> font;
        stbtt_fontinfo info{};

    public:
        explicit Font(const char *name) {
            FILE *f = fopen(name, "rb");
            if (f == nullptr) {
                size_t size;
                void *data = gZip->open(name, size);
                if (data == nullptr) {
                    printf("failed to open font file: %s\n", name);
                    exit(-1);
                }
                font.resize(size);
                memcpy(font.data(), data, size);
                gZip->close(data);
            } else {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);
                font.resize(size);
                fread(&font[0], 1, size, f);
                fclose(f);
            }

            stbtt_InitFont(&info, font.data(), 0);
        }

        void makeBitmap(wchar_t code, float size, std::vector<unsigned char> &bitmap, int &x0, int &y0, int &w,
                        int &h) const {
            float scale = stbtt_ScaleForPixelHeight(&info, size);
            int x1, y1;
            stbtt_GetCodepointBitmapBox(&info, code, scale, scale, &x0, &y0, &x1, &y1);
            w = x1 - x0;
            h = y1 - y0;
            bitmap.resize(w * h);
            stbtt_MakeCodepointBitmap(&info, bitmap.data(), w, h, w, scale, scale, code);
        }
    };

    class Audio {
        struct Vorbis {
            stb_vorbis *vorbis = nullptr;
            int loop = 0;
            int pause = 0;
            void *data = nullptr;
        };

        static constexpr int MAX_AUDIO = 5;
        SDL_AudioDeviceID audioDeviceID;
        Vorbis vorbis[MAX_AUDIO];
        std::vector<short> samples[MAX_AUDIO];

        int findVorbis() const {
            for (int i = 0; i < MAX_AUDIO; ++i) {
                if (vorbis[i].pause == 0) {
                    return i;
                }
            }
            return -1;
        }

    public:
        Audio() {
            SDL_AudioSpec spec;
            spec.freq = 44100;
            spec.format = AUDIO_S16;
            spec.channels = 2;
            spec.samples = 1024;
            spec.callback = nullptr;
            spec.userdata = nullptr;
            audioDeviceID = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
        }

        ~Audio() {
            SDL_PauseAudioDevice(audioDeviceID, 1);
            SDL_CloseAudioDevice(audioDeviceID);
            for (const auto &item: vorbis) {
                stb_vorbis_close(item.vorbis);
                gZip->close(item.data);
            }
        }

        int open(const char *name, const int loop = 1) {
            int idx = findVorbis();
            if (idx == -1) {
                return -1;
            }
            int error = 0;
            vorbis[idx].vorbis = stb_vorbis_open_filename(name, &error, nullptr);
            if (vorbis[idx].vorbis == nullptr) {
                size_t size;
                void *data = gZip->open(name, size);
                if (data == nullptr) {
                    printf("failed to open file: %s\n", name);
                    exit(-1);
                }
                vorbis[idx].data = data;
                vorbis[idx].vorbis = stb_vorbis_open_memory(reinterpret_cast<const unsigned char *>(data),
                                                            static_cast<int>(size), &error, nullptr);
            }
            vorbis[idx].loop = loop;
            vorbis[idx].pause = 0;
            return idx;
            // SDL_PauseAudioDevice(audioDeviceID, 0);
        }

        void close(const int idx) {
            stb_vorbis_close(vorbis[idx].vorbis);
            gZip->close(vorbis[idx].data);
            vorbis[idx] = Vorbis();
        }

        void play() {
            Uint32 queuedAudioSize = SDL_GetQueuedAudioSize(audioDeviceID);
            if (queuedAudioSize == 0) {
                for (int i = 0; i < MAX_AUDIO; ++i) {
                    if (!vorbis[i].vorbis || vorbis[i].pause) {
                        continue;
                    }
                    samples[i].resize(2048);
                    int ret = stb_vorbis_get_samples_short_interleaved(vorbis[i].vorbis, 2, samples[i].data(),
                                                                       static_cast<int>(samples[i].size()));
                    if (ret == 0) {
                        stb_vorbis_seek_start(vorbis[i].vorbis);
                        if (!vorbis[i].loop) {
                            vorbis[i].pause = 1;
                        }
                    }
                }
                std::vector<short> samples_mix(2048);
                for (size_t k = 0; k < samples_mix.size(); ++k) {
                    int sample = 0;
                    for (int i = 0; i < MAX_AUDIO; ++i) {
                        if (!vorbis[i].vorbis || vorbis[i].pause) {
                            continue;
                        }
                        sample += samples[i][k];
                    }
                    constexpr int s16max = static_cast<short>(0x7FFF);
                    constexpr int s16min = static_cast<short>(0x8000);
                    if (sample > s16max) {
                        sample = s16max;
                    }
                    if (sample < s16min) {
                        sample = s16min;
                    }
                    samples_mix[k] = static_cast<short>(sample);
                }
                SDL_QueueAudio(audioDeviceID, samples_mix.data(),
                               static_cast<Uint32>(samples_mix.size()) * sizeof(short));
            }
        }

        void pause(int pause) const {
            SDL_PauseAudioDevice(audioDeviceID, pause);
        }
    };

    Audio* gAudio;

    int lua_error_callback(lua_State *L) {
        const char *error = lua_tostring(L, 1);
        luaL_traceback(L, L, error, 0);
        return 1;
    }

    int lua_ziploader(lua_State* L) {
        std::string name = luaL_checkstring(L, 1);
        for (size_t i = 0; i < name.size(); ++i) {
            if (name[i] == '.') { // 不能用双引号：""自带\0，要用''
                name[i] = '/';
            }
        }
        name += ".lua";
        size_t size;
        void* p = gZip->open(name.c_str(), size);
        if (p == nullptr) {
            luaL_error(L, "%s not found!", name.c_str());
            return 0;
        }
        lua_pushcfunction(L, lua_error_callback);
        int ret = luaL_loadbuffer(L, static_cast<const char *>(p), size, name.c_str());
        if (ret == 0) {
            ret = lua_pcall(L, 0, 1, -2);
        }
        if (ret) {
            printf("%s\n", lua_tostring(L, -1));
        }
        gZip->close(p);
        return 1;
    }
    int lua_glClearColor(lua_State *L) {
        glClearColor(static_cast<GLfloat>(luaL_checknumber(L, 1)),
                     static_cast<GLfloat>(luaL_checknumber(L, 2)),
                     static_cast<GLfloat>(luaL_checknumber(L, 3)),
                     static_cast<GLfloat>(luaL_checknumber(L, 4)));
        return 0;
    }

    int lua_glClear(lua_State *L) {
        glClear(static_cast<GLbitfield>(luaL_checkinteger(L, 1)));
        return 0;
    }

    int lua_glViewport(lua_State *L) {
        glViewport(static_cast<GLint>(luaL_checkinteger(L, 1)),
                   static_cast<GLint>(luaL_checkinteger(L, 2)),
                   static_cast<GLsizei>(luaL_checkinteger(L, 3)),
                   static_cast<GLsizei>(luaL_checkinteger(L, 4)));
        return 0;
    }

    int lua_glDrawArrays(lua_State *L) {
        auto mode = static_cast<GLenum>(luaL_checkinteger(L, 1));
        auto first = static_cast<GLint>(luaL_checkinteger(L, 2));
        auto count = static_cast<GLsizei>(luaL_checkinteger(L, 3));
        glDrawArrays(mode, first, count);
        return 0;
    }
    int lua_glEnable(lua_State* L) {
        auto cap = static_cast<GLenum>(luaL_checkinteger(L, 1));
        glEnable(cap);
        return 0;
    }
    int lua_glDisable(lua_State* L) {
        auto cap = static_cast<GLenum>(luaL_checkinteger(L, 1));
        glDisable(cap);
        return 0;
    }
    int lua_glBlendFunc(lua_State* L) {
        auto sfactor = static_cast<GLenum>(luaL_checkinteger(L, 1));
        auto dfactor = static_cast<GLenum>(luaL_checkinteger(L, 2));
        glBlendFunc(sfactor, dfactor);
        return 0;
    }
    template<typename T>
    void pushObject(lua_State *L, T *obj, const char *name) {
        auto **udata = static_cast<T **>(lua_newuserdata(L, sizeof(T**)));
        *udata = obj;
        luaL_getmetatable(L, name);
        lua_setmetatable(L, -2);
    }

    int lua_newShader(lua_State *L) {
        const char *vsSrc = luaL_checkstring(L, 1);
        const char *fsSrc = luaL_checkstring(L, 2);
        auto *shader = new Shader(vsSrc, fsSrc);
        pushObject(L, shader, "Shader");
        return 1;
    }

    int lua_newBuffer(lua_State *L) {
        Buffer *buffer;
        if (lua_istable(L, 1)) {
            std::vector<float> point;
            const size_t len = lua_rawlen(L, 1);
            point.resize(len);
            for (size_t i = 1; i <= len; ++i) {
                lua_rawgeti(L, 1, i);

                point[i - 1] = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);
            }
            buffer = new Buffer(point);
        } else {
            const auto x = static_cast<float>(luaL_checknumber(L, 1));
            const auto y = static_cast<float>(luaL_checknumber(L, 2));
            const auto w = static_cast<float>(luaL_checknumber(L, 3));
            const auto h = static_cast<float>(luaL_checknumber(L, 4));
            const bool uv = lua_isnone(L, 5) ? true : lua_toboolean(L, 5);
            buffer = new Buffer(x, y, w, h, uv);
        }
        pushObject(L, buffer, "Buffer");
        return 1;
    }

    int lua_newTexture(lua_State *L) {
        Texture* texture;
        if (lua_gettop(L) == 1) {
            const char *name = luaL_checkstring(L, 1);
            texture = new Texture(name);

        }else {
            std::vector<unsigned char> bitmap;
            size_t size;
            const char* p = luaL_checklstring(L, 1, &size);
            bitmap.resize(size);
            memcpy(bitmap.data(), p, size);
            const int w = static_cast<int>(luaL_checkinteger(L, 2));
            const int h = static_cast<int>(luaL_checkinteger(L, 3));
            texture = new Texture(bitmap, w, h);
        }
        pushObject(L, texture, "Texture");
        return 1;
    }

    int lua_newFont(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        auto* font = new Font(name);
        pushObject(L, font, "Font");
        return 1;
    }

    int lua_audioOpen(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        int loop = lua_isnone(L, 2) ? 1 : luaL_checkinteger(L, 2);
        int idx = gAudio->open(name, loop);
        lua_pushinteger(L, idx);
        return 1;

    }

    int lua_audioClose(lua_State* L) {
            int idx = static_cast<int>(luaL_checkinteger(L, 1));
        gAudio->close(idx);
        return 0;
    }

    int lua_audioPause(lua_State* L) {
        int pause = static_cast<int>(luaL_checkinteger(L, 1));
        gAudio->pause(pause);
        return 0;
    }
    template<typename T>
    int lua_object_gc(lua_State *L) {
        T **shader = static_cast<T **>(lua_touserdata(L, 1));
        delete *shader;
        return 0;
    }

    int lua_shader_attrib(lua_State *L) {
        auto **udata = static_cast<Shader **>(luaL_checkudata(L, 1, "Shader"));
        const char *name = luaL_checkstring(L, 2);
        const auto size = static_cast<GLint>(luaL_checkinteger(L, 3));
        const auto type = static_cast<GLenum>(luaL_checkinteger(L, 4));
        const auto stride = static_cast<GLsizei>(luaL_optinteger(L, 5, 0));
        const void *pointer = lua_isnone(L, 6) ? nullptr : reinterpret_cast<void *>(lua_tointeger(L, 6));
        (*udata)->attrib(name, size, type, stride, pointer);
        return 0;
    }

    int lua_shader_setVec4(lua_State *L) {
        auto **udata = static_cast<Shader **>(luaL_checkudata(L, 1, "Shader"));
        const char *name = luaL_checkstring(L, 2);
        const auto v0 = static_cast<float>(luaL_checknumber(L, 3));
        const auto v1 = static_cast<float>(luaL_checknumber(L, 4));
        const auto v2 = static_cast<float>(luaL_checknumber(L, 5));
        const auto v3 = static_cast<float>(luaL_checknumber(L, 6));
        (*udata)->setVec4(name, v0, v1, v2, v3);
        return 0;
    }

    int lua_shader_setTexture(lua_State *L) {
        auto **udata = static_cast<Shader **>(luaL_checkudata(L, 1, "Shader"));
        const char *name = luaL_checkstring(L, 2);
        const auto texture = static_cast<GLint>(luaL_checkinteger(L, 3));
        (*udata)->setTexture(name, texture);
        return 0;
    }

    int lua_shader_use(lua_State *L) {
        auto **udata = static_cast<Shader **>(luaL_checkudata(L, 1, "Shader"));
        (*udata)->use();
        return 0;
    }

    const luaL_Reg shader_meta[] =
    {
        {"_gc", lua_object_gc<Shader>},
        {"attrib", lua_shader_attrib},
        {"setVec4", lua_shader_setVec4},
        {"setTexture", lua_shader_setTexture},
        {"use", lua_shader_use},
        // {0, 0},
        {nullptr, nullptr},
    };

    int lua_buffer_bind(lua_State *L) {
        auto **udata = static_cast<Buffer **>(luaL_checkudata(L, 1, "Buffer"));
        (*udata)->bind();
        return 0;
    };

    int lua_buffer_unbind(lua_State *L) {
        auto **udata = static_cast<Buffer **>(luaL_checkudata(L, 1, "Buffer"));
        (*udata)->unbind();
        return 0;
    };
    const luaL_Reg buffer_meta[] =
    {
        {"_gc", lua_object_gc<Buffer>},
        // {0, 0},
        {"bind", lua_buffer_bind},
        {"unbind", lua_buffer_unbind},
        {nullptr, nullptr},
    };

    int lua_texture_bind(lua_State *L) {
        auto **udata = static_cast<Texture **>(luaL_checkudata(L, 1, "Texture"));
        const auto texture = static_cast<GLint>(luaL_checkinteger(L, 2));
        (*udata)->bind(texture);
        return 0;
    };

    int lua_texture_unbind(lua_State *L) {
        auto **udata = static_cast<Texture **>(luaL_checkudata(L, 1, "Texture"));
        (*udata)->unbind();
        return 0;
    }

    const luaL_Reg texture_meta[] ={
        {"__gc", lua_object_gc<Texture>},
        {"bind", lua_texture_bind},
        {"unbind", lua_texture_unbind},
        {nullptr, nullptr},
    };

    int lua_font_makeBitmap(lua_State* L) {
        auto** udata = static_cast<Font**>(luaL_checkudata(L, 1, "Font"));
        const auto code = static_cast<wchar_t>(luaL_checkinteger(L, 2));
        const auto size = static_cast<float>(luaL_checknumber(L, 3));
        std::vector<unsigned char> bitmap;
        int x0, y0, w, h;
        (*udata)->makeBitmap(code, size, bitmap, x0, y0, w, h);
        lua_pushlstring(L, reinterpret_cast<const char*>(bitmap.data()), bitmap.size());
        lua_pushinteger(L, x0);
        lua_pushinteger(L, y0);
        lua_pushinteger(L, w);
        lua_pushinteger(L, h);
        return 5;
    }

    const luaL_Reg font_meta[] = {
        {"__gc", lua_object_gc<Font>},
        {"makeBitmap", lua_font_makeBitmap},
        {nullptr, nullptr},
    };
    void makeObject(lua_State *L, const char *name, const luaL_Reg *meta) {
        luaL_newmetatable(L, name);
        luaL_setfuncs(L, meta, 0);
        lua_pushstring(L, "__index");
        lua_pushvalue(L, -2);
        lua_rawset(L, -3);
        lua_pop(L, 1);
    }

    class Lua {
        lua_State *L;
        bool nextCall = true;

        void init() {
            lua_pushinteger(L, winW);
            lua_setglobal(L, "winW");
            lua_pushinteger(L, winH);
            lua_setglobal(L, "winH");

            mouseEvent(0, 0, 0, 0);
            keyEvent(0, 0);
            lua_pushcfunction(L, lua_glClearColor);
            lua_setglobal(L, "glClearColor");
            lua_pushcfunction(L, lua_glClear);
            lua_setglobal(L, "glClear");
            lua_pushcfunction(L, lua_glViewport);
            lua_setglobal(L, "glViewport");
            lua_pushcfunction(L, lua_glDrawArrays);
            lua_setglobal(L, "glDrawArrays");
            lua_pushcfunction(L, lua_glEnable);
            lua_setglobal(L, "glEnable");
            lua_pushcfunction(L, lua_glDisable);
            lua_setglobal(L, "glDisable");
            lua_pushcfunction(L, lua_glBlendFunc);
            lua_setglobal(L, "glBlendFunc");


            lua_pushcfunction(L, lua_newShader);
            lua_setglobal(L, "newShader");
            makeObject(L, "Shader", shader_meta);

            lua_pushcfunction(L, lua_newBuffer);
            lua_setglobal(L, "newBuffer");
            makeObject(L, "Buffer", buffer_meta);

            lua_pushcfunction(L, lua_newTexture);
            lua_setglobal(L, "newTexture");
            makeObject(L, "Texture", texture_meta);
            lua_pushcfunction(L, lua_buffer_bind);
            lua_setglobal(L, "buffer_bind");
            lua_pushcfunction(L, lua_buffer_unbind);
            lua_setglobal(L, "buffer_unbind");

            lua_pushcfunction(L, lua_newFont);
            lua_setglobal(L, "newFont");
            makeObject(L, "Font", font_meta);
            lua_pushcfunction(L, lua_font_makeBitmap);
            lua_setglobal(L, "font_makeBitmap");

            lua_pushcfunction(L, lua_audioOpen);
            lua_setglobal(L, "audioOpen");
            lua_pushcfunction(L, lua_audioClose);
            lua_setglobal(L, "audioClose");
            lua_pushcfunction(L, lua_audioPause);
            lua_setglobal(L, "audioPause");

        }

    public:
        Lua() {
            L = luaL_newstate();
            luaL_openlibs(L);

            init();

            lua_pushcfunction(L, lua_ziploader);
            lua_setglobal(L, "ziploader");
            lua_pushcfunction(L, lua_error_callback);
            int ret = luaL_loadstring(
                L,
                "table.insert(package.searchers, function() return ziploader end)\n"
                // "require 'data.main'");
                "require 'main'");
            if (ret == 0) {
                ret = lua_pcall(L, 0, 0, -2);
            }
            if (ret) {
                printf("%s\n", lua_tostring(L, -1));
                nextCall = false;
            }
        }

        ~Lua() {
            lua_close(L);
        }

        void draw() {
            if (!nextCall) {
                return;
            }
            lua_getglobal(L, "draw");
            int ret = lua_pcall(L, 0, 0, -2);
            if (ret) {
                printf("%s\n", lua_tostring(L, -1));
                nextCall = false;
            }
        }

        void mouseEvent(Uint32 event, Sint32 x, Sint32 y, Uint8 button) {
            lua_pushinteger(L, event);
            lua_setglobal(L, "MouseEvent");
            lua_pushinteger(L, x);
            lua_setglobal(L, "MouseX");
            lua_pushinteger(L, y);
            lua_setglobal(L, "MouseY");
            lua_pushinteger(L, button);
            lua_setglobal(L, "MouseButton");
        };
        void keyEvent(Uint32 event, SDL_Keycode code) {
            lua_pushinteger(L, event);
            lua_setglobal(L, "KeyEvent");
            lua_pushinteger(L, code);
            lua_setglobal(L, "KeyCode");
        }
        void clearEvents() {
            lua_pushinteger(L, 0);
            lua_setglobal(L, "MouseEvent");
            lua_pushinteger(L, 0);
            lua_setglobal(L, "KeyEvent");
    };
};

}

int main(int argc, char **argv) {
    printf("mini2d\n");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window *window = SDL_CreateWindow("mini2d", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH,
                                          SDL_WINDOW_OPENGL);
    const SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return -1;
    }

    printf("GL_VERSION:%s\n", reinterpret_cast<const char *>(glGetString(GL_VERSION)));

    gZip = new Zip("../data/data.zip");

    auto _checkGLError = [](const char *file, const int line) {
        for (GLint error = glGetError(); error != GL_NO_ERROR; error = glGetError()) {
            printf("[%s][%d]: 0x%04x\n", file, line, error);
        }
    };

#define checkGLError() _checkGLError(__FILE__, __LINE__)

    const auto vsSrc = R"(
        #version 330 core
        attribute vec2 position;
        void main() {
            float x = 2.0 * position.x / (640.0 - 1.0) - 1.0;
            float y = 1.0 - 2.0 * position.y / (480.0 - 1.0);
            gl_Position = vec4(x, y, 0.0, 1.0);
        }
    )";

    const auto fsSrc = R"(
        #version 330 core
        uniform vec4 color;
        void main() {
            // gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
            gl_FragColor = color;
        }
    )";

    Shader shader(vsSrc, fsSrc);
    const GLuint programID = shader.getID();


    const auto vsSrcUV = R"(
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
    )";

    const auto fsSrcUV = R"(
        #version 330 core
        uniform vec4 color;
        uniform sampler2D texture0;
        varying vec2 uv;
        void main() {
            vec4 texcolor = texture2D(texture0, uv);
            gl_FragColor = color * texcolor;
        }
    )";

    const Shader shaderUV(vsSrcUV, fsSrcUV);
    const GLuint programIDUV = shaderUV.getID();

    const Buffer buffer(150, 50, 300, 300, true);
    const Buffer bufferRect(150, 50, 350, 350, false);
    const Buffer bufferPoint(std::vector<float>{50, 100,});
    const Buffer bufferLine(std::vector<float>{0, 0, static_cast<float>(winW) - 1, static_cast<float>(winH) - 1});

    Texture texture("data/uvchecker.png");

    Font font("data/AlibabaPuHuiTi-3-55-Regular.ttf");
    int x0, y0, w, h;
    std::vector<unsigned char> bitmap(w * h);
    font.makeBitmap(*L"啊", 50, bitmap, x0, y0, w, h);

    Texture textureFont(bitmap, w, h);

    Buffer bufferFont(150, 50, static_cast<float>(w), static_cast<float>(h));
    const auto fsSrcFont = R"(
        #version 330 core
        uniform vec4 color;
        uniform sampler2D texture0;
        varying vec2 uv;
        void main() {
            vec4 texcolor = texture2D(texture0, uv);
            gl_FragColor = vec4(color.rgb, color.a * texcolor.a);
        }
    )";

    Shader shaderfont(vsSrcUV, fsSrcFont);

    auto draw = [&]() {
        glClearColor(0.5, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0, 0, winW, winH);

        auto drawPoint = [&](const Buffer &buffer, const Shader &shader) {
            buffer.bind();
            shader.attrib("position", 2, GL_FLOAT);
            shader.use();
            shader.setVec4("color", 1.0f, 0.0f, 0.0f, 1.0f);
            glDrawArrays(GL_POINTS, 0, 2);
            buffer.unbind();
        };
        auto drawLine = [&](const Buffer &buffer, const Shader &shader) {
            buffer.bind();
            shader.attrib("position", 2, GL_FLOAT);
            shader.use();
            shader.setVec4("color", 0.0f, 1.0f, 0.0f, 1.0f);
            glDrawArrays(GL_LINES, 0, 4);
            buffer.unbind();
        };
        auto drawRect = [&](const Buffer &buffer, const Shader &shader) {
            buffer.bind();
            shader.attrib("position", 2, GL_FLOAT);
            shader.use();
            shader.setVec4("color", 0.0f, 0.0f, 1.0f, 1.0f);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            buffer.unbind();
        };
        auto drawRectUV = [&](const Buffer &buffer, const Shader &shader, const Texture &texture) {
            buffer.bind();
            shader.attrib("position", 2, GL_FLOAT, 4 * sizeof(float), nullptr);
            shader.attrib("texcoord", 2, GL_FLOAT, 4 * sizeof(float),
                          reinterpret_cast<const void *>(2 * sizeof(float)));
            shader.use();
            shader.setVec4("color", 1.0f, 1.0f, 1.0f, 1.0f);
            shader.setTexture("texture0", 0);
            texture.bind(0);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            texture.unbind();
            buffer.unbind();
        };
        drawPoint(bufferPoint, shader);
        drawLine(bufferLine, shader);
        drawRect(bufferRect, shader);
        drawRectUV(buffer, shaderUV, texture);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawRectUV(bufferFont, shaderfont, textureFont);
        glDisable(GL_BLEND);
    };
    // Audio audio;
    // audio.open("data/MeetingTheStars.ogg");
    // audio.open("data/SadSoul.ogg");
    // audio.pause(0);

    gAudio = new Audio();


    Lua lua;

    int done = 0;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                done = 1;
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                lua.mouseEvent(event.type, event.button.x, event.button.y, event.button.button);
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                lua.mouseEvent(event.type, event.button.x, event.button.y, event.button.button);
            } else if (event.type == SDL_MOUSEMOTION) {
                lua.mouseEvent(event.type, event.motion.x, event.motion.y, 0);
            } else if (event.type == SDL_KEYDOWN) {
                lua.keyEvent(event.type, event.key.keysym.sym);
            } else if (event.type == SDL_KEYUP) {
                lua.keyEvent(event.type, event.key.keysym.sym);
            }
        }
        lua.draw();
        lua.clearEvents();
        // audio.play();

        gAudio->play();
        checkGLError();
        SDL_GL_SwapWindow(window);
    };
    SDL_DestroyWindow(window);
    delete gAudio;
    delete gZip;
    return 0;
}
