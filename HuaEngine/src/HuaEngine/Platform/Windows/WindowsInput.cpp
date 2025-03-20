#include "enginepch.h"
#include "WindowsInput.h"
#include "GLFW/glfw3.h"
#include "HuaEngine/Application.h"

HE::Input* HE::Input::m_Instance = new WindowsInput();

bool HE::WindowsInput::IsKeyPressedImpl(KeyCode keycode)
{
    auto window = static_cast<GLFWwindow*>(Application::GetInstance().GetWindow().GetNativeWindow());
    auto status = glfwGetKey(window, (int)keycode);
    return status == GLFW_PRESS || status == GLFW_REPEAT;
}

bool HE::WindowsInput::IsMousePressedImpl(MouseCode keycode)
{
    auto window = static_cast<GLFWwindow*>(Application::GetInstance().GetWindow().GetNativeWindow());
    auto status = glfwGetMouseButton(window, (int)keycode);
    return status == GLFW_PRESS;
}

float HE::WindowsInput::GetMouseXImpl()
{
    auto window = static_cast<GLFWwindow*>(Application::GetInstance().GetWindow().GetNativeWindow());
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    return (float)xpos;
}

float HE::WindowsInput::GetMouseYImpl()
{
    auto window = static_cast<GLFWwindow*>(Application::GetInstance().GetWindow().GetNativeWindow());
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    return (float)ypos;
}
