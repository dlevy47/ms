#include "gfx/sprite.hh"

namespace gfx {

Error Sprite::Frame::load(
    Sprite::Frame* self,
    wz::Image image,
    const uint8_t* image_data) {
    self->image = image;

    glGenTextures(1, &self->texture);
    glBindTexture(GL_TEXTURE_2D, self->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        image.width,
        image.height,
        0,
        self->format(),
        self->type(),
        image_data);

    return Error();
}

Error Sprite::Frame::loadfromfile(
    Sprite::Frame* self,
    const wz::OpenedFile::Node* node) {
    const auto canvas = std::get_if<wz::OpenedFile::Canvas>(&node->value);
    if (canvas == nullptr) {
        return error_new(Error::FRAMELOADFAILED)
            << "frame node is not a canvas";
    }

    CHECK(load(
        self,
        canvas->image,
        canvas->image_data),
        Error::FRAMELOADFAILED)
        << "failed to load frame from image data";

    // Read in origin and delay, if they exist. Errors here
    // are ignored.
    self->origin.x = 0;
    self->origin.y = 0;
    self->delay = 100;

    node->childvector(
        L"origin", &self->origin.x, &self->origin.y);
    node->childint32(
        L"delay", &self->delay);

    if (self->delay < 0)
        self->delay = 0;

    return Error();
}

void Sprite::Frame::quad(
    Vector<int32_t> at,
    gfx::Vertex vertices[4]) const {
    vertices[0].position[0] =
        static_cast<float>(at.x - origin.x);
    vertices[0].position[1] =
        static_cast<float>(at.y - origin.y);
    vertices[0].uv[0] = 0;
    vertices[0].uv[1] = 0;

    vertices[1].position[0] =
        static_cast<float>(at.x + static_cast<int32_t>(image.width) - origin.x);
    vertices[1].position[1] =
        static_cast<float>(at.y - origin.y);
    vertices[1].uv[0] = 1;
    vertices[1].uv[1] = 0;

    vertices[2].position[0] =
        static_cast<float>(at.x + static_cast<int32_t>(image.width) - origin.x);
    vertices[2].position[1] =
        static_cast<float>(at.y + static_cast<int32_t>(image.height) - origin.y);
    vertices[2].uv[0] = 1;
    vertices[2].uv[1] = 1;

    vertices[3].position[0] =
        static_cast<float>(at.x - origin.x);
    vertices[3].position[1] =
        static_cast<float>(at.y + static_cast<int32_t>(image.height) - origin.y);
    vertices[3].uv[0] = 0;
    vertices[3].uv[1] = 1;
}

Error Sprite::loadfromfile(
    Sprite* self,
    const wz::OpenedFile::Node* node) {
    auto it = node->iterator();

    // TODO: are we guaranteed that sprite frames are in order?
    const wz::OpenedFile::Node* frame_node = nullptr;
    while ((frame_node = it.next())) {
        // Only consider frame nodes that are integers.
        {
            std::wstringstream ss(frame_node->name);
            int32_t num = 0;
            ss >> num;

            if (ss.fail())
                continue;
        }

        if (const auto* uol = std::get_if<wz::OpenedFile::Uol>(&frame_node->value)) {
            frame_node = frame_node->parent->find(uol->uol);
        }

        Frame frame;
        CHECK(Frame::loadfromfile(
            &frame,
            frame_node),
            Error::SPRITELOADFAILED)
            << "failed to load sprite frame";

        self->frames.emplace_back(std::move(frame));
    }

    return Error();
}

}
