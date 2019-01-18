

/// Setting up build systems is unnecessarily complicated. IDEs expose lots of 
/// individual settings in mysterious locations that can vary from release to
/// release. Command line build systems are plagued by poor documentation, ad hoc
/// scripting languages, and a relative lack of portability between platforms.
///
/// The focus of this book is on the architecture of our program, not on how it
/// is built. In that spirit, cmake has been selected as the simultaneously least 
/// broken and most portable of the command line systems as it is a meta-build
/// system - it builds reasonably functional build scripts in a platform's native
/// dialect, whether it be make files, xcode projects, or visual studio solutions.
///
/// We won't concern ourselves at the moment with the contents of the provided
/// cmake script, but to get started let's do two things.
///
/// First, let's generate a version of this file readable in a browser. We'll need
/// stddoc, from here: https://github.com/r-lyeh/stddoc.c  Compile it, and then at 
/// a command line, type 
/// ~~~~
/// stddoc < chapter1.cpp > chapter1.html
/// ~~~~
/// Then open chapter1.html in a browser. Next, at a development command line, go to 
/// a directory you'd like to do your build in. I suggest a sibling to the gusteau
/// directory,and type
///
/// ~~~~
/// mkdir gusteau-build-chapter1
/// cd gusteau-build-chapter1
/// cmake ../gusteau -DCHAPTER=chapter1
/// ~~~~
/// Note that for this to work properly on Windows, the development command line
/// must match the build you want. So run the Development Tools for x64 command
/// line that came with the version of visual studio you are using. The command
/// line for cmake on windows will actually need to look like -
/// ~~~~
/// cmake ..\gusteau -G "Visual Studio 15 2017 Win64"
/// ~~~~
///
/// Similarly on mac, you'll need to use xcode-select to pick the compiler you
/// want cmake to use.
///
/// When cmake completes go ahead and build the resulting project.
///
///
/// Separate the state of the program from the user interface
/// Separate rendering from the state of the program
/// Program state should be updated via a channel

#include <deque>
#include <functional>
#include <string>
#include <cstring>
#include <memory>

/// The application context manages the lifespan of the objects that make up the system.
///
/// The state context holds the application state. This object is a simple
/// declaration for the application to hold on to. It will be up to us to sub-class
/// it in interesting ways.
///<C++
class StateContext
{
    struct Detail;
    Detail* m_{};
public:
    StateContext();
    ~StateContext();
};

/// There's no need to use a unique_ptr for Detail, the context object destructor
/// is responsible for removing it. Using a unique_ptr requires exposing the
/// contents of the Detail class, and so must be globally available, or 
/// known to the context object destructor. Since the idea is to not make it
/// globally available, and since the context destructor will have to know 
/// about it anyway, the unique_ptr adds no value.
///
/// The detail objects can be lazily created by the state object when
/// needed. Having a simple pointer here makes the context object itself
/// lightweight and trivially constructable.
///<C++
class RenderContext
{
    struct Detail;
    Detail* m_{};
public:
    RenderContext();
    ~RenderContext();
};

/// Various systems are going to need access to the graphics context for
/// rendering. The details are not important, the only thing that matters
/// is that it can be declared and held by objects that reference it,
/// such as the ApplicationContext.
///<C++
class GraphicsContext
{
    struct Detail;
    Detail* m_{};
public:
    GraphicsContext();
    virtual ~GraphicsContext();
};

class UIContext;

class ApplicationContextBase
{
public:
    ApplicationContextBase() {}
    virtual ~ApplicationContextBase() = default;
    virtual void Update() = 0;
    bool join_now = false;
    std::shared_ptr<UIContext> ui;
};

class UIContext
{
    struct Detail;
    Detail* m_{};
public:
    UIContext(GraphicsContext& context,
        const std::string& window_name, int width, int height);
    ~UIContext();
    void Render(ApplicationContextBase&);
    virtual void Run(ApplicationContextBase&) = 0;
};

/// Factory functions are used to create contexts. The application's main() is the
/// one designated owner of the created contexts, since it will outlast all references
/// to the contexts.
/// Factory functions are used so that we can fill them in later without having
/// to rebuild or modify the application itself in any way.
///<C++
std::unique_ptr<GraphicsContext> CreateRootGraphicsContext();
std::shared_ptr<UIContext> CreateUIContext(GraphicsContext&);
std::shared_ptr<ApplicationContextBase> CreateApplicationContext(GraphicsContext& gc, std::shared_ptr<UIContext> ui);
///>


/// The application context bundles the context objects, and the running state
/// of the application itself

#ifdef GUSTEAU_chapter1
///<C++
class ApplicationContext : public ApplicationContextBase
{
public:	
    ApplicationContext(GraphicsContext& gc,std::shared_ptr<UIContext> ui_) 
        : root_graphics_context(gc)
    {
        ui = ui_;
    }

    ~ApplicationContext() = default;

    virtual void Update() override {}    // doesn't need to do anything for chapter 1

    GraphicsContext& root_graphics_context;
    StateContext state;
    RenderContext render;

    bool join_now{};
};

std::shared_ptr<ApplicationContextBase> CreateApplicationContext(GraphicsContext& gc, std::shared_ptr<UIContext> ui)
{
    return std::make_shared<ApplicationContext>(gc, ui);
}

///>
#endif // GUSTEAU_chapter1

#include <thread>
#include <vector>
#include <iostream>

/// We'll name the engines up front, and come back to them in a moment.
///<C++
void StateEngine(std::shared_ptr<ApplicationContextBase>);
void RenderEngine(std::shared_ptr<ApplicationContextBase>);
void UIEngine(std::shared_ptr<ApplicationContextBase>);

/// It will be the job of the UIEngine to set the join_now flag on the context
/// which will let all the other engines know it's time to shut down.
///
/// The main function will own the root graphics context and application context
/// for the duration of the executation of the application. We'll signal this
/// intent to the reader of the code by either instantiating these objects on the
/// stack, or holding them in a unique_ptr. We'll only pass references to these
/// objects, further signalling to the reader that the objects are not owned.
///
/// The main program scopes an ApplicationContext and vector of threads that
/// exist for the duration of the program's run.
///
/// The threads launched by main capture a reference to the ApplicationContext
/// and will exit when the join_now flag is set on the context.
///
/// main() doesn't have much else to do, except to run until finished. It will
/// block on join and sleep until then.
///
/// All of main is guarded by a try/catch block so that if anything goes wrong
/// during the run, we have a place to trap and report it.
///<C++
int main(int argc, char** argv) try
{
    std::unique_ptr<GraphicsContext> root_graphics_context(CreateRootGraphicsContext());
    std::shared_ptr<UIContext> ui_context = CreateUIContext(*root_graphics_context.get());
    std::shared_ptr<ApplicationContextBase> app_context(CreateApplicationContext(*root_graphics_context.get(), ui_context));

    std::vector<std::thread> jobs(2);
    jobs.emplace_back(std::thread([app_context]() { StateEngine(app_context); }));
    jobs.emplace_back(std::thread([app_context]() { RenderEngine(app_context); }));

    UIEngine(app_context);

    for (auto& j : jobs)
        if (j.joinable())
            j.join();



    return 0;
}
catch (std::exception& exc)
{
    std::cerr << "Unexpected termination due to " << exc.what() << std::endl;
    return 1;
}

/// Next, let's head over to the UIContext to see what's up.
///
/// The UIContext is going to hold everything the user can see, starting with
/// the window.
/// GLEW will provide the GL bindings.
///<C++
#include <GL/glew.h>

/// For this demonstration, the window will be maintained by GLFW.
///<C++
#include <GLFW/glfw3.h>

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error %d: %s\n", error, description);
}

/// THe glfw flags should be set consistently and to a relatively modern version,
/// so a function will be provided up front for that.
/// Detail is going to maintain a root graphics context that all GL contexts
/// can share. This will be useful later when more than one rendering system
/// is in place.
///<C++
void setGlfwFlags()
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
}

/// The GraphicsContext object will contain an invisible root window
/// with a GL context that can be shared with other GL windows and offscreen
/// render contexts, to enable sharing of resources like textures and
/// vertex data.
///<C++
struct GraphicsContext::Detail
{
    Detail() = default;
    Detail(Detail&&) = default;
    ~Detail()
    {
    }
};
///>

GraphicsContext::GraphicsContext()
: m_(new Detail)
{
}

GraphicsContext::~GraphicsContext()
{
    delete m_;
}

/// The GLFWGraphicsContext exists to ensure that there is a single, root, shared
/// rendering context that all systems can use. If all the graphics contexts are
/// created sharing this context, then shareable resources such as vertex buffers,
/// shader programs, and textures only need to be instantiated once, and then used
/// in all render contexts.
/// Some resources are specific to a context. In OpenGL, an example of such as resource
/// is a VAO. The details of sharing are outside the scope of this tutorial, so please
/// refer to the appropriate documentation to learn more.
///<C++
class GLFWGraphicsContext : public GraphicsContext
{
public:
    GLFWGraphicsContext()
    {
        glfwSetErrorCallback(error_callback);
        if (!glfwInit())
            throw std::runtime_error("Could not initialize glfw");

        setGlfwFlags();
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

        GLFWwindow* window = glfwCreateWindow(16, 16, "Root graphics context", NULL, NULL);
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1); // must be set when a window's context is current

        glewExperimental = GL_TRUE;
        glewInit(); // create GLEW after the context has been created

        if (!GLEW_VERSION_4_1)
            std::cerr << "glew didn't init properly" << std::endl;

        if (!glProgramParameteri)
            std::cerr << "glew didn't init properly" << std::endl;

        // get version info
        const GLubyte* renderer = glGetString(GL_RENDERER); // get renderer string
        const GLubyte* version = glGetString(GL_VERSION); // version as a string
        std::cout << "OpenGL Renderer: " << renderer << std::endl;
        std::cout << "OpenGL Version: " << version << std::endl;

        _window = window;

        // the context should only be captured when it's in use, as it can't be
        // captured in two places at once
        glfwMakeContextCurrent(nullptr);
    }

    virtual ~GLFWGraphicsContext()
    {
        if (_window)
            glfwDestroyWindow(_window);
    }
    GLFWwindow* _window{};
};

/// This factory function exists to allow the details of the graphics context
/// to be declared at the latest possible moment. It also makes it possible for the
/// application itself to be agnostic to the rendering system in use, since it
/// would be possible to substitute another graphics system in this portion of the
/// code without needing to recompile the main application.
///<C++
std::unique_ptr<GraphicsContext> CreateRootGraphicsContext()
{
    return std::make_unique<GLFWGraphicsContext>();
}

/// The UIContext will contain the main user interface window.
/// We're going to use Imgui, so Imgui's context information will be
/// stored here.
/// ImGui doesn't yet have public support for managing multiple contexts,
/// so this implementation reaches into the internals to accomplish that.
/// The details aren't important, and are subject to change.
///<C++
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h> // for ImGuiContext
#include <imgui/examples/imgui_impl_glfw.h>
#include <imgui/examples/imgui_impl_opengl3.h>

struct UIContext::Detail
{
    GLFWwindow* _window{};
    ImGuiContext* _context{};
    std::string _window_name;
    std::function<void(ApplicationContextBase&)> _run;

    Detail(GraphicsContext& context,
           std::function<void(ApplicationContextBase&)> run,
           const std::string& window_name, int width, int height)
        : _window_name(window_name), _run(run)
    {
        setGlfwFlags();
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

        GLFWGraphicsContext* gc = dynamic_cast<GLFWGraphicsContext*>(&context);

        _window = glfwCreateWindow(width, height, window_name.c_str(), NULL, gc->_window);
        glfwMakeContextCurrent(_window);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        _context = ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsClassic();

        // Setup Platform/Renderer bindings
        const char* glsl_version = "#version 150";
        ImGui_ImplGlfw_InitForOpenGL(_window, true);
        ImGui_ImplOpenGL3_Init(glsl_version);
    
        // the context should only be captured when it's in use, as it can't be
        // captured in two places at once
        glfwMakeContextCurrent(nullptr);
    }

    ~Detail()
    {
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(_window);
        glfwTerminate();
    }

    void ActivateContext()
    {
        ImGuiContext* prevContext = ImGui::GetCurrentContext();
        if (prevContext == _context)
            return;

        if (prevContext != nullptr)
        {
            std::memcpy(&_context->Style, &prevContext->Style, sizeof(ImGuiStyle));
            std::memcpy(&_context->IO.KeyMap, &prevContext->IO.KeyMap, sizeof(prevContext->IO.KeyMap));
            //std::memcpy(&_context->MouseCursorData, &prevContext->MouseCursorData, sizeof(_context->MouseCursorData));
            _context->IO.IniFilename = prevContext->IO.IniFilename;
            _context->IO.RenderDrawListsFn = prevContext->IO.RenderDrawListsFn;
            _context->Initialized = prevContext->Initialized;
        }

        ImGui::SetCurrentContext(_context);
    }

    void Render(ApplicationContextBase& context)
    {
        if (!_window || context.join_now)
            return;

        glfwMakeContextCurrent(_window);

        ActivateContext();

        int w, h;
        glfwGetFramebufferSize(_window, &w, &h);

        auto& io = ImGui::GetIO();
        // Setup display size (every frame to accommodate for window resizing)
        io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        //ImGuizmo::BeginFrame();

        // make a full screen window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w), static_cast<float>(h)));
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoFocusOnAppearing
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        // ensure every window has a unique id
        char buff[256];
        sprintf(buff, "###GraphicsWindow_%td", (ptrdiff_t)this);
        ImGui::Begin(buff, 0, flags);

        const float font_scale = 1.0f;
        ImGui::SetWindowFontScale(font_scale);

        // Rendering
        glViewport(0, 0, w, h);

        ImVec4 clear_color = ImColor(1, 0, 0, 1);

        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (_run) _run(context);

        ImGui::End(); // end the main window
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(_window);
    }
};
///>

UIContext::UIContext(GraphicsContext& context,
    const std::string& window_name, int width, int height)
    : m_(new Detail(context, [this](ApplicationContextBase& ac) { Run(ac); }, window_name, width, height))
{
}

UIContext::~UIContext()
{
	delete m_;
}

void UIContext::Render(ApplicationContextBase& context) { m_->Render(context); }



void UIEngine(std::shared_ptr<ApplicationContextBase> context)
{
    while (!context->join_now)
    {
        // if the graphics viewport is not actively rendering, update at 24hz
        // When the render engine is in place, and has an animation mode,
        // this timeout should be set appropriately to the intended frame rate.
        // We'll come back to this later.
        glfwWaitEventsTimeout(1.f / 24.f);
        context->ui->Render(*context.get());
        context->Update();
    }
}

struct StateContext::Detail
{};

StateContext::StateContext()
: m_(new Detail()) {}

StateContext::~StateContext()
{
    delete m_;
}

void StateEngine(std::shared_ptr<ApplicationContextBase>) {}

struct RenderContext::Detail
{};

RenderContext::RenderContext()
    : m_(new Detail()) {}

RenderContext::~RenderContext()
{
    delete m_;
}

void RenderEngine(std::shared_ptr<ApplicationContextBase>) {}

/// Everything up until now has been structure to run our application, and
/// will be the same for every application that follows this outline.
/// All that's left is the specialization of the individual pieces.

#ifdef GUSTEAU_chapter1

///<C++
class GusteauChapter1UI : public UIContext
{
public:
    GusteauChapter1UI(GraphicsContext& gc,
        const std::string& window_name, int width, int height)
        : UIContext(gc, window_name, width, height)
        {}

    virtual void Run(ApplicationContext& ac) override
    {
        ImGui::Text("Hello world");
        if (ImGui::Button("Quit"))
        {
            ac.join_now = true;
        }
    }
};

std::unique_ptr<UIContext> CreateUIContext(GraphicsContext& gc)
{
    auto ui = std::make_unique<GusteauChapter1UI>(gc, "gusteau", 1024, 1024);
    return ui;
}
///>

#endif
