#include "demo.hh"

#include "util.hh"

static gfx::Vector<double> screen_to_ndc(gfx::Vector<int> window_size, gfx::Vector<double> at) {
  return gfx::Vector<double> {
    .x = ((at.x * 2) / window_size.x) - 1,
    .y = 1 - ((at.y * 2) / window_size.y),
  };
}

static void Demo_feedinput(
  Demo* self,
  gl::Window* window,
  gfx::Vector<int32_t> window_size) {
  self->input.start();

  self->input.area(Demo::CLIENT_MAP, gfx::Rect<double>{
    .topleft = {.x = 0, .y = 0},
    .bottomright = (gfx::Vector<double>) window_size,
  });

  for (const nk_window* cur = self->ui.context->begin; cur; cur = cur->next) {
    self->input.area(Demo::CLIENT_NK, gfx::Rect<double>{
      .topleft = {
        .x = (double) cur->bounds.x,
        .y = (double) cur->bounds.y,
      },
      .bottomright = {
        .x = (double) (cur->bounds.x + cur->bounds.w),
        .y = (double) (cur->bounds.y + cur->bounds.h),
      },
    });
  }

  gfx::Vector<double> mouse;

  glfwGetCursorPos(
    window->window,
    &mouse.x,
    &mouse.y);

  self->input.input(ui::Input::InputEvent {
    .event = ui::Input::InputEvent::MouseMotion {
      .at = (gfx::Vector<double>) mouse,
    },
  });

  if (window->scroll.available) {
    self->input.input(ui::Input::InputEvent {
      .event = ui::Input::InputEvent::MouseScroll {
        .scroll = window->scroll.value,
      },
    });
  }

  while (!window->mouse_buttons.empty()) {
    gl::Window::MouseButton mb =
      window->mouse_buttons.front();

    self->input.input(ui::Input::InputEvent {
      .event = ui::Input::InputEvent::MouseButton {
        .state = (mb.action == gl::Window::MouseButton::PRESS ? ui::Input::InputEvent::MouseButton::DOWN : ui::Input::InputEvent::MouseButton::UP),
        .button = mb.button,
      },
    });

    window->mouse_buttons.pop();
  }

  while (!window->key_codepoints.empty()) {
    self->input.input(ui::Input::InputEvent {
      .event = ui::Input::InputEvent::Codepoint {
        .codepoint = window->key_codepoints.front(),
      },
    });

    window->key_codepoints.pop();
  }

  while (!window->keys.empty()) {
    auto keypress = window->keys.front();
    ui::Key key = ui::KEY_NONE;

    switch (keypress.key) {
      case GLFW_KEY_BACKSPACE:
        key = ui::KEY_BACKSPACE;
        break;
      default:
        break;
    }

    if (keypress.action == GLFW_PRESS) {
      self->input.input(ui::Input::InputEvent {
        .event = ui::Input::InputEvent::KeyDown {
          .key = key,
        },
      });
    } else {
      self->input.input(ui::Input::InputEvent {
        .event = ui::Input::InputEvent::KeyUp {
          .key = key,
        },
      });
    }

    window->keys.pop();
  }
}

static void Demo_dooutput_map(
  Demo* self,
  const ui::Input::OutputEvent* ev,
  gfx::Vector<int> window_size,
  const client::game::Renderer::Target* target) {
  // The map is only interested in drags and scrolls.
  std::visit(Visitor {
    [&](const ui::Input::OutputEvent::Drag& d) {
      gfx::Vector<double> map_lmp = target->unproject(
        screen_to_ndc(window_size, d.from));
      gfx::Vector<double> map_tmp = target->unproject(
        screen_to_ndc(window_size, d.to));

      gfx::Vector<double> translate = map_lmp - map_tmp;
      self->map_viewport.translate(translate);
    },
    [&](const ui::Input::OutputEvent::MouseScroll& s) {
      gfx::Vector<double> map_tmp = target->unproject(
        screen_to_ndc(window_size, s.at));

      gfx::Vector<double> scroll = s.scroll;
      if (scroll.y > 10) scroll.y = 10;
      if (scroll.y < -10) scroll.y = -10;

      scroll.y *= -.04;
      scroll.y += 1;

      // Limit area of the viewport to cap zoom.
      if (scroll.y < 1 && self->map_viewport.area() < (0.01 * self->map_state->basemap.bounding_box.area())) {
        // Area is too small, and wants to be zoomed in.
      } else if (scroll.y > 1 && self->map_viewport.area() > (10 * self->map_state->basemap.bounding_box.area())) {
        // Area is too big, and wants to be zoomed out.
      } else {
        self->map_viewport.scale_about(scroll.y, map_tmp);
      }
    },
    [](auto) {},
  }, ev->event);
}

static void Demo_dooutput_nk(
  Demo* self,
  const ui::Input::OutputEvent* ev) {
  std::visit(Visitor {
    [&](const ui::Input::OutputEvent::MouseOver& o) {
      nk_input_motion(
        self->ui.context,
        o.at.x,
        o.at.y);
    },
    [&](const ui::Input::OutputEvent::MouseDown& u) {
      if (u.button == GLFW_MOUSE_BUTTON_LEFT) {
        nk_input_button(
          self->ui.context,
          NK_BUTTON_LEFT,
          (int) u.at.x,
          (int) u.at.y,
          true);
      }
    },
    [&](const ui::Input::OutputEvent::MouseUp& u) {
      if (u.button == GLFW_MOUSE_BUTTON_LEFT) {
        nk_input_button(
          self->ui.context,
          NK_BUTTON_LEFT,
          (int) u.at.x,
          (int) u.at.y,
          false);
      }
    },
    [&](const ui::Input::OutputEvent::MouseScroll& s) {
      nk_input_scroll(
        self->ui.context,
        {
          .x = (float) s.scroll.x,
          .y = (float) s.scroll.y,
        });
    },
    [&](const ui::Input::OutputEvent::KeyDown& k) {
      enum nk_keys nk = NK_KEY_NONE;
      switch (k.key) {
        case ui::KEY_BACKSPACE:
          nk = NK_KEY_BACKSPACE;
          break;
        case ui::KEY_NONE:
          return;
      }

      nk_input_key(
        self->ui.context,
        nk,
        1);
    },
    [&](const ui::Input::OutputEvent::KeyUp& k) {
      enum nk_keys nk = NK_KEY_NONE;
      switch (k.key) {
        case ui::KEY_BACKSPACE:
          nk = NK_KEY_BACKSPACE;
          break;
        case ui::KEY_NONE:
          return;
      }

      nk_input_key(
        self->ui.context,
        nk,
        0);
    },
    [&](const ui::Input::OutputEvent::Codepoint& c) {
      nk_input_unicode(
        self->ui.context,
        c.codepoint);
    },
    [](auto) {},
  }, ev->event);
}

static void Demo_dooutput(
  Demo* self,
  gfx::Vector<int> window_size,
  const client::game::Renderer::Target* target) {
  nk_input_begin(self->ui.context);
  while (!self->input.output.empty()) {
    const ui::Input::OutputEvent& ev = self->input.output.front();

    if (ev.client == Demo::CLIENT_MAP) {
      Demo_dooutput_map(
        self,
        &ev,
        window_size,
        target);
    } else if (ev.client == Demo::CLIENT_NK) {
      Demo_dooutput_nk(
        self,
        &ev);
    }

    self->input.output.pop_front();
  }
  nk_input_end(self->ui.context);
}

void Demo::processinput(
  gl::Window* window,
  const client::game::Renderer::Target* target) {
  gfx::Vector<int> window_size;

  window->size(
    &window_size.x,
    &window_size.y);

  Demo_feedinput(
    this,
    window,
    window_size);

  Demo_dooutput(
    this,
    window_size,
    target);
}
