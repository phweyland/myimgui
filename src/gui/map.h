static inline void map_keyboard(GLFWwindow *w, int key, int scancode, int action, int mods)
{
  if(action == GLFW_PRESS)
  {
    if(key == GLFW_KEY_L)
      dt_view_switch(s_view_lighttable);
  }
}
