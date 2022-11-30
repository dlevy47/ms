#include <algorithm>
#include <fstream>
#include <iostream>

#include "lodepng/lodepng.h"

#include "util/error.hh"
#include "wz/vfs.hh"
#include "wz/wz.hh"

#include "gl.hh"
#include "browser.hh"

static void Browser_ui_fromfilenode(
        Browser* self,
        const wz::OpenedFile* of,
        const wz::OpenedFile::Node* node,
        std::wstring name_stack) {
    name_stack += L"-";

    // The root node doesn't have a name.
    if (node->name)
        name_stack += node->name;

    for (uint32_t i = 0; i < node->children.count; ++i) {
        const wz::OpenedFile::Node* child = &node->children.start[i];

        std::wstringstream label;
        label << child->name;
        switch (child->kind) {
            case wz::Property::VOID:
                label << " = [void]";
                break;
            case wz::Property::UINT16:
                label << " = [u16] " << child->uint16;
                break;
            case wz::Property::INT32:
                label << " = [i32] " << child->int32;
                break;
            case wz::Property::FLOAT32:
                label << " = [f32] " << child->float32;
                break;
            case wz::Property::FLOAT64:
                label << " = [f64] " << child->float64;
                break;
            case wz::Property::STRING:
                label << " = [str] " << child->string;
                break;
            case wz::Property::VECTOR:
                label << " = [vec] " << child->vector[0] << ", " << child->vector[1];
                break;
            case wz::Property::SOUND:
                label << " = [snd]";
                break;
            case wz::Property::UOL:
                label << " = [uol] " << child->uol;
                break;
            case wz::Property::CANVAS:
                label << " = [img] "
                    << (child->canvas.image.is_encrypted() ? "E " : "C ")
                    << child->canvas.image.width << " x " << child->canvas.image.height << " "
                    << "(" << child->canvas.image.format << " " << static_cast<uint32_t>(child->canvas.image.format2) << ")";
                break;
            default:
                break;
        }

        if (child->children.count || child->kind == wz::Property::CANVAS) {
            std::wstring this_stack = name_stack + L"-" + child->name;

            if (nk_tree_push_hashed(
                        self->ui.context,
                        NK_TREE_NODE,
                        self->converter.to_bytes(label.str()).c_str(),
                        NK_MINIMIZED,
                        reinterpret_cast<const char*>(this_stack.c_str()),
                        this_stack.size() * sizeof(wchar_t),
                        0)) {
                if (child->kind == wz::Property::CANVAS) {
                    nk_layout_row_dynamic(self->ui.context, 0, 2); {
                        if (nk_button_label(self->ui.context, "Save")) {
                            std::vector<uint8_t> png_data;
                            unsigned err = lodepng::encode(png_data, child->canvas.image_data, child->canvas.image.width, child->canvas.image.height);
                            if (err) {
                                std::cerr << "failed to encode image data to PNG: " << err << "\n";
                            } else {
                                std::wstring filename = this_stack + L".png";
                                std::wcout << L"writing " << filename << L"\n";
                                std::basic_ofstream<char> outf(
                                        self->converter.to_bytes(filename).c_str(),
                                        std::ios::binary);
                                outf.write(
                                        reinterpret_cast<char*>(png_data.data()),
                                        png_data.size());
                            }
                        }

                        if (nk_button_label(self->ui.context, "Show")) {
                            std::unique_ptr<gfx::Sprite::Frame> new_frame =
                                std::make_unique<gfx::Sprite::Frame>();

                            Error err = gfx::Sprite::Frame::loadfromfile(
                                    new_frame.get(),
                                    child);
                            if (err) {
                                std::wcerr << L"failed to load frame: " << err << "\n";
                            } else {
                                self->frame.swap(new_frame);
                            }
                        }
                    }
                }

                Browser_ui_fromfilenode(
                        self,
                        of,
                        child,
                        this_stack);

                nk_tree_pop(self->ui.context);
            }
        } else {
            nk_layout_row_dynamic(self->ui.context, 0, 1); {
                nk_label(self->ui.context, self->converter.to_bytes(label.str()).c_str(), NK_TEXT_LEFT);
            }

            Browser_ui_fromfilenode(
                    self,
                    of,
                    &node->children.start[i],
                    name_stack);
        }
    }
}

static void Browser_ui_fromvfsnode(
        Browser* self,
        wz::Vfs::Node* node,
        std::wstring name_stack) {
    name_stack += L"-" + node->name;

    if (nk_tree_push_hashed(
                self->ui.context,
                NK_TREE_NODE,
                self->converter.to_bytes(node->name).c_str(),
                NK_MINIMIZED,
                reinterpret_cast<const char*>(name_stack.c_str()),
                name_stack.size() * sizeof(wchar_t),
                0)) {
        if (wz::Vfs::Directory* directory = node->directory()) {
            std::vector<wz::Vfs::Node*> children;

            for (auto& it : directory->children) {
                children.push_back(&it.second);
            }

            std::sort(
                    children.begin(),
                    children.end(),
                    [](wz::Vfs::Node*& left, wz::Vfs::Node*& right) {
                    return left->name < right->name;
                    });

            for (size_t i = 0, l = children.size(); i < l; ++i) {
                Browser_ui_fromvfsnode(self, children[i], name_stack);
            }
        } else if (wz::Vfs::File* file = node->file()) {
            if (file->rc) {
                nk_layout_row_dynamic(self->ui.context, 0, 2); {
                    nk_label(self->ui.context, "Opened File", NK_TEXT_LEFT);

                    if (nk_button_label(self->ui.context, "Close")) {
                        file->close();
                    }
                }
            } else {
                nk_layout_row_dynamic(self->ui.context, 0, 2); {
                    nk_label(self->ui.context, "Unopened File", NK_TEXT_LEFT);

                    if (nk_button_label(self->ui.context, "Open")) {
                        Error e = file->open(nullptr);
                        if (e) {
                            std::cerr << "failed to open file: ";
                            e.print(std::wcerr);
                            std::cerr << "\n";
                        }
                    }
                }
            }

            if (file->rc) {
                Browser_ui_fromfilenode(self, file->opened.get(), &file->opened->nodes[0], name_stack + L"-properties");
            }
        }

        nk_tree_pop(self->ui.context);
    }
}

Error Browser::init(
        Browser* self,
        const std::vector<std::string>& argv,
        const char* window_title) {
    self->browsed_files.resize(argv.size() - 1);
    for (size_t i = 1; i < argv.size(); ++i) {
        BrowsedFile* bf = &self->browsed_files[i - 1];

        bf->filename = argv[i];

        std::cerr << "loading wz " << argv[i] << "\n";
        CHECK(wz::Wz::open(&bf->wz, argv[i].c_str()),
                Error::OPENFAILED) << "failed to open " << argv[i].c_str();
        std::cerr << "wz loaded\n";

        std::cerr << "loading vfs\n";
        CHECK(wz::Vfs::open(&bf->vfs, &bf->wz),
                Error::OPENFAILED) << "failed to build vfs";
        std::cerr << "vfs loaded \n";
    }

    CHECK(gl::Window::init(
                &self->window,
                window_title),
            Error::UIERROR) << "failed to initialize window";

    CHECK(client::game::Renderer::init(
                &self->game_renderer),
            Error::UIERROR) << "failed to initialize game renderer";

    CHECK(nk::Ui::init(
                &self->ui),
            Error::UIERROR) << "failed to initialize nk ui";

    CHECK(client::ui::NkRenderer::init(
                &self->nk_renderer),
            Error::UIERROR) << "failed to initialize nk renderer";

    return Error();
}

Error Browser::run() {
    float frame_position[2] = {0, 0};
    double last_mouse_position[2] = {0, 0};

    while (!window.shouldclose()) {
        gfx::Vector<int> window_size;
        gfx::Vector<int> framebuffer_size;
        window.size(
                &window_size.x,
                &window_size.y);
        window.framebuffer_size(
                &framebuffer_size.x,
                &framebuffer_size.y);

        struct nk_vec2 scale = {
            .x = (float) framebuffer_size.x / (float) window_size.x,
            .y = (float) framebuffer_size.y / (float) window_size.y,
        };

        ui.input(&window, false);

        if(nk_begin(ui.context, "WZ explorer", nk_rect(0, 0, window_size.x / 3, window_size.y),
                    NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | NK_WINDOW_BACKGROUND)) {
            nk_layout_row_dynamic(ui.context, 0, 1);

            for (size_t i = 0; i < browsed_files.size(); ++i) {
                std::wstringstream wss;
                wss << browsed_files[i].filename.c_str();
                Browser_ui_fromvfsnode(this, &browsed_files[i].vfs.root, wss.str());
            }
        }
        nk_end(ui.context);

        glClear(GL_COLOR_BUFFER_BIT);

        {
            gfx::Rect<uint32_t> viewport = {0};
            viewport.bottomright = (gfx::Vector<uint32_t>) framebuffer_size;

            gl::Window::Frame frame;

            CHECK(window.frame(
                        &frame,
                        viewport),
                    Error::UIERROR) << "failed to start frame";

            // Draw content.
            if (this->frame) {
                // Since we always draw images at 0,0 in game coordinates,
                // just provide a window-sized rectangle centered around 0,0.
                gfx::Rect<double> game_viewport = {
                    .topleft = {
                        .x = static_cast<double>(- window_size.x / 2),
                        .y = static_cast<double>(- window_size.y / 2),
                    },
                    .bottomright = {
                        .x = static_cast<double>(window_size.x / 2),
                        .y = static_cast<double>(window_size.y / 2),
                    },
                };

                client::game::Renderer::Target target =
                    game_renderer.begin(
                            &frame,
                            game_viewport);

                gfx::Vector<int32_t> at = {0};
                target.frame(
                        this->frame.get(),
                        at);
            }

            // Draw UI.
            nk_renderer.render(
                    &frame,
                    window_size,
                    &ui);
        }
    }

    return Error();
}
