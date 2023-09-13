#pragma once

#include <deque>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "gfx/rect.hh"

namespace ui {

enum Key {
  KEY_NONE,
  KEY_BACKSPACE,
};

// Input multiplexes user input to clients, maintaining important state like
// current focus. Input is used by updating the list of window areas before 
// each frame's input phase, i.e., on each frame:
//   1. Populate Input with window areas per client
//   2. Feed Input with input events
//   3. Retrieve events per client
//   4. Each client processes as appropriate.
// Clients are identified by 32-bit non-zero integers. These values are opaque to
// the Input instance so it is up to the user of the Input to assign meaning. The
// zero value is reserved for use by the Input instance.
struct Input {
  // ClientID is a typedef for types that hold client IDs.
  typedef int32_t ClientID;
  
  // MouseButton is a typedef for types that hold mouse button IDs.
  typedef int32_t MouseButton;
  
  // Area is a client window area.
  struct Area {
    gfx::Rect<double> area;
    ClientID client;
  };

  // InputEvent is an input event.
  struct InputEvent {
    struct MouseMotion {
      gfx::Vector<double> at;
    };
    
    struct MouseButton {
      enum {
        UP,
        DOWN,
      } state;
      
      Input::MouseButton button;
    };

    struct MouseScroll {
      gfx::Vector<double> scroll;
    };

    struct KeyDown {
      Key key;
    };

    struct KeyUp {
      Key key;
    };

    struct Codepoint {
      uint32_t codepoint;
    };

    std::variant<
      MouseMotion,
      MouseButton,
      MouseScroll,
      KeyDown,
      KeyUp,
      Codepoint> event;
  };

  // OutputEvent is a translated output event.
  struct OutputEvent {
    struct MouseOver {
      gfx::Vector<double> at;
    };
    
    struct MouseDown {
      MouseButton button;
      gfx::Vector<double> at;
    };

    struct MouseUp {
      MouseButton button;
      gfx::Vector<double> at;
      bool was_drag;
    };

    struct MouseScroll {
      gfx::Vector<double> scroll;
      gfx::Vector<double> at;
    };

    struct DragStart {
      MouseButton button;
      gfx::Vector<double> at;
    };

    struct Drag {
      MouseButton button;

      gfx::Vector<double> from;

      gfx::Vector<double> to;
      
      gfx::Vector<double> diff() const {
        return to - from;
      }
    };

    struct DragEnd {
      MouseButton button;
      gfx::Vector<double> at;
    };

    struct KeyDown {
      Key key;
    };

    struct KeyUp {
      Key key;
    };

    struct Codepoint {
      uint32_t codepoint;
    };

    ClientID client;

    std::variant<
      MouseOver,
      MouseDown,
      MouseUp,
      MouseScroll,
      DragStart,
      Drag,
      DragEnd,
      KeyDown,
      KeyUp,
      Codepoint> event;
  };

  // DownMouseButtonState is inter-frame state stored on for mouse buttons
  // that were clicked.
  struct DownMouseButtonState {
    // drag_target is the current target of a drag started by this
    // mouse button.
    ClientID drag_target;

    // is_drag represents whether this really was a drag. It may have just been
    // a click.
    bool is_drag;

    // last is the last location of the mouse cursor while this button was down.
    gfx::Vector<double> last;
  };

  // mouse_at is the last reported location of the mouse.
  gfx::Vector<double> mouse_at;

  std::unordered_map<MouseButton, DownMouseButtonState> mouse_down_states;

  // focused is the ID of the client with focus. Focus is used to determine which
  // client receives:
  //   * Keyboard events
  ClientID focused;

  // clients is a per-frame list of seen client IDs.
  std::unordered_set<ClientID> clients;

  // areas is the per-frame list of client window areas.
  // TODO: Use a better data structure here.
  std::vector<Area> areas;

  // output is the output event queue.
  std::deque<OutputEvent> output;

  // start starts a new frame for the Input. Any intra-frame state is cleared.
  void start() {
    clients.clear();
    areas.erase(areas.begin(), areas.end());
    output.clear();
  }

  // at returns the client at the provided location, or 0 if there is no client.
  ClientID at(gfx::Vector<double> p) const;

  // area adds a new area to the Input. Areas should be inserted bottom up (i.e. in
  // draw order).
  void area(ClientID client, gfx::Rect<double> area) {
    clients.insert(client);
    areas.push_back(Area {
      .area = area,
      .client = client
    });
  }

  // input feeds an input event.
  void input(InputEvent ev);
};

std::ostream& operator<<(std::ostream& os, const Input::OutputEvent& ev);

}
