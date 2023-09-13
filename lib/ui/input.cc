#include "ui/input.hh"

#include "util.hh"

namespace ui {

Input::ClientID Input::at(gfx::Vector<double> p) const {
  // Because areas is constructed in draw order, go backwards through
  // the list.
  for (auto it = areas.rbegin(); it != areas.rend(); ++it) {
    if (it->area.contains(p)) {
      return it->client;
    }
  }
  
  return 0;
}

static void Input_input_MouseMotion(
  Input* self,
  const Input::InputEvent::MouseMotion* ev) {
  self->mouse_at = ev->at;

  std::unordered_set<Input::ClientID> dragged_clients;

  for (auto& state : self->mouse_down_states) {
    if (state.second.drag_target == 0 || !self->clients.contains(state.second.drag_target)) {
      continue;
    }

    gfx::Vector<double> diff = self->mouse_at - state.second.last;
    if (diff.x == 0 && diff.y == 0) {
      continue;
    }

    if (!state.second.is_drag) {
      // Drag started.
      self->output.push_back(Input::OutputEvent {
        .client = state.second.drag_target,
        .event = Input::OutputEvent::DragStart {
          .button = state.first,
          .at = self->mouse_at,
        },
      });
    }

    self->output.push_back(Input::OutputEvent {
      .client = state.second.drag_target,
      .event = Input::OutputEvent::Drag {
        .button = state.first,
        .from = state.second.last,
        .to = self->mouse_at,
      },
    });
    
    state.second.last = self->mouse_at;
    state.second.is_drag = true;
    dragged_clients.insert(state.second.drag_target);
  }

  Input::ClientID target = self->at(ev->at);
  if (target && !dragged_clients.contains(target)) {
    self->output.push_back(Input::OutputEvent {
      .client = target,
      .event = Input::OutputEvent::MouseOver {
        .at = ev->at,
      },
    });
  }
}

static void Input_input_MouseButton(
  Input* self,
  const Input::InputEvent::MouseButton* ev) {
  if (ev->state == Input::InputEvent::MouseButton::DOWN) {
    Input::ClientID target = self->at(self->mouse_at);
    if (!target) {
      // No target here.
      return;
    }

    self->focused = target;

    self->mouse_down_states.insert_or_assign(ev->button, Input::DownMouseButtonState{
      .drag_target = target,
      .last = self->mouse_at,
    });

    self->output.push_back(Input::OutputEvent {
      .client = target,
      .event = Input::OutputEvent::MouseDown {
        .button = ev->button,
        .at = self->mouse_at,
      },
    });
  } else if (ev->state == Input::InputEvent::MouseButton::UP) {
    Input::ClientID target = self->at(self->mouse_at);
    bool was_drag = false;

    const auto& state = self->mouse_down_states.find(ev->button);
    if (state != self->mouse_down_states.end()) {
      // Close out this drag.
      // TODO: Do something here?
      was_drag = state->second.is_drag;

      if (was_drag && self->clients.contains(state->second.drag_target)) {
        self->output.push_back(Input::OutputEvent {
          .client = state->second.drag_target,
          .event = Input::OutputEvent::DragEnd {
            .button = ev->button,
            .at = self->mouse_at,
          },
        });
      }
    }

    self->mouse_down_states.erase(ev->button);

    if (target) {
      self->output.push_back(Input::OutputEvent {
        .client = target,
        .event = Input::OutputEvent::MouseUp {
          .button = ev->button,
          .at = self->mouse_at,
          .was_drag = was_drag,
        },
      });
    }
  }
}

static void Input_input_MouseScroll(
  Input* self,
  const Input::InputEvent::MouseScroll* ev) {
  // Scroll events always go to the hovered area.
  Input::ClientID target = self->at(self->mouse_at);
  if (target == 0) {
    return;
  }

  self->output.push_back(Input::OutputEvent {
    .client = target,
    .event = Input::OutputEvent::MouseScroll {
      .scroll = ev->scroll,
      .at = self->mouse_at,
    },
  });
}

static void Input_input_Key(
  Input* self,
  const Key k,
  bool down) {
  if (self->focused == 0 || !self->clients.contains(self->focused)) {
    return;
  }

  if (down) {
    self->output.push_back(Input::OutputEvent {
      .client = self->focused,
      .event = Input::OutputEvent::KeyDown {
        .key = k,
      },
    });
  } else {
    self->output.push_back(Input::OutputEvent {
      .client = self->focused,
      .event = Input::OutputEvent::KeyUp {
        .key = k,
      },
    });
  }
}

static void Input_input_Codepoint(
  Input* self,
  const Input::InputEvent::Codepoint& ev) {
  if (self->focused == 0 || !self->clients.contains(self->focused)) {
    return;
  }

  self->output.push_back(Input::OutputEvent {
    .client = self->focused,
    .event = Input::OutputEvent::Codepoint {
      .codepoint = ev.codepoint,
    },
  });
}

void Input::input(Input::InputEvent in) {
  std::visit(Visitor {
    [&](const InputEvent::MouseMotion& ev) {
      Input_input_MouseMotion(this, &ev);
    },
    [&](const InputEvent::MouseButton& ev) {
      Input_input_MouseButton(this, &ev);
    },
    [&](const InputEvent::MouseScroll& ev) {
      Input_input_MouseScroll(this, &ev);
    },
    [&](const InputEvent::KeyDown& ev) {
      Input_input_Key(this, ev.key, true);
    },
    [&](const InputEvent::KeyUp& ev) {
      Input_input_Key(this, ev.key, false);
    },
    [&](const InputEvent::Codepoint& ev) {
      Input_input_Codepoint(this, ev);
    },
  }, in.event);
}

std::ostream& operator<<(std::ostream& os, const Input::OutputEvent& ev) {
  std::visit(Visitor {
    [&](const Input::OutputEvent::MouseOver& d) {
      os << "MouseOver(" << "to " << ev.client << ") {";
      os << "at: " << d.at;
      os << "}";
    },
    [&](const Input::OutputEvent::MouseDown& d) {
      os << "MouseDown(" << "to " << ev.client << ") {";
      os << "button: " << d.button << ", ";
      os << "at: " << d.at;
      os << "}";
    },
    [&](const Input::OutputEvent::MouseUp& u) {
      os << "MouseUp(" << "to " << ev.client << ") {";
      os << "button: " << u.button << ", ";
      os << "at: " << u.at << ", ";
      os << "was_drag: " << (u.was_drag ? "true" : "false");
      os << "}";
    },
    [&](const Input::OutputEvent::MouseScroll& s) {
      os << "MouseScroll(" << "to " << ev.client << ") {";
      os << "scroll: " << s.scroll << ", ";
      os << "at: " << s.at << ", ";
      os << "}";
    },
    [&](const Input::OutputEvent::DragStart& d) {
      os << "DragStart(" << "to " << ev.client << ") {";
      os << "button: " << d.button << ", ";
      os << "at: " << d.at;
      os << "}";
    },
    [&](const Input::OutputEvent::Drag& d) {
      os << "Drag(" << "to " << ev.client << ") {";
      os << "from: " << d.from << ", ";
      os << "to: " << d.to << ", ";
      os << "diff: " << d.diff();
      os << "}";
    },
    [&](const Input::OutputEvent::DragEnd& d) {
      os << "DragEnd(" << "to " << ev.client << ") {";
      os << "button: " << d.button << ", ";
      os << "at: " << d.at;
      os << "}";
    },
    [&](const Input::OutputEvent::KeyDown& k) {
      os << "KeyDown(" << "to " << ev.client << ") {";
      os << "key: " << k.key;
      os << "}";
    },
    [&](const Input::OutputEvent::KeyUp& k) {
      os << "KeyUp(" << "to " << ev.client << ") {";
      os << "key: " << k.key;
      os << "}";
    },
    [&](const Input::OutputEvent::Codepoint& c) {
      os << "Codepoint(" << "to " << ev.client << ") {";
      os << "codepoint: " << c.codepoint;
      os << "}";
    },
  }, ev.event);

  return os;
}

}
