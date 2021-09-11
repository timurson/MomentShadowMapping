#include "framebuffer.h"

using std::vector;
using std::domain_error;
using std::invalid_argument;
using std::out_of_range;

#include <iostream>
using std::cout;
using std::endl;

FrameBuffer::FrameBuffer()
    :
    width(0),
    height(0),
    frame_id(0),
    depth_id(0),
    stencil_id(0),
    buffers(0)
{
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_color_attachments);
    buffers = new GLenum[max_color_attachments];
    glGenFramebuffers(1, &frame_id);

}

FrameBuffer::FrameBuffer(int width_, int height_)
    :
    width(0),
    height(0),
    frame_id(0),
    depth_id(0),
    stencil_id(0),
    buffers(0)
{
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_color_attachments);
    buffers = new GLenum[max_color_attachments];
    glGenFramebuffers(1, &frame_id);
    width = width_;
    height = height_;
}

FrameBuffer::~FrameBuffer()
{
    GLuint tex_id;
    vector<GLuint>::const_iterator cii;
    for (cii = tex_ids.begin(); cii != tex_ids.end(); cii++)
    {
        tex_id = *cii;
        glDeleteTextures(1, &tex_id);
    }

    if (depth_id)
    {
        glDeleteRenderbuffers(1, &depth_id);
    }

    if (stencil_id)
    {
        glDeleteRenderbuffers(1, &stencil_id);
    }
    glDeleteFramebuffers(1, &frame_id);
    delete[] buffers;
}

void FrameBuffer::attachRender(GLenum iformat, bool multisample) throw (domain_error, invalid_argument)
{
    GLenum attachment;
    GLuint render_id;

    if (width == 0 || height == 0)
    {
        throw domain_error("FrameBuffer::AttachRender - one of the dimensions is zero");
    }

    if (iformat == GL_DEPTH_COMPONENT24 || iformat == GL_DEPTH_COMPONENT)
    {
        attachment = GL_DEPTH_ATTACHMENT;
    }
    else if (iformat == GL_STENCIL_INDEX1 || iformat == GL_STENCIL_INDEX4 || iformat == GL_STENCIL_INDEX8 ||
        iformat == GL_STENCIL_INDEX16 || iformat == GL_STENCIL_INDEX)
    {
        attachment = GL_STENCIL_ATTACHMENT;
    }
    else if (iformat == GL_DEPTH24_STENCIL8 || iformat == GL_DEPTH_STENCIL)
    {
        attachment = GL_DEPTH_STENCIL_ATTACHMENT;
    }
    else
    {
        throw invalid_argument("FrameBuffer::AttachRender - unrecognized internal format");
    }

    glGenRenderbuffers(1, &render_id);
    glBindFramebuffer(GL_FRAMEBUFFER, frame_id);
    glBindRenderbuffer(GL_RENDERBUFFER, render_id);
    if (multisample)
    {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH24_STENCIL8, width, height);
    }
    else
    {
        glRenderbufferStorage(GL_RENDERBUFFER, iformat, width, height);
    }
    
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, render_id);

    if (attachment == GL_DEPTH_ATTACHMENT || attachment == GL_DEPTH_STENCIL_ATTACHMENT)
    {
        depth_id = render_id;
    }
    else if (attachment == GL_STENCIL_ATTACHMENT)
    {
        stencil_id = render_id;
    }

}

void FrameBuffer::attachTexture(GLenum iformat, GLint filter) throw(domain_error, out_of_range, invalid_argument)
{
    GLenum format;
    GLenum type;
    GLenum attachment;
    GLuint tex_id;

    if (width == 0 || height == 0) {
        throw domain_error("FrameBuffer::attachTexture - one of the dimensions is zero");
    }

    if (int(tex_ids.size()) == max_color_attachments) {
        throw out_of_range("FrameBuffer::attachTexture - GL_MAX_COLOR_ATTACHMENTS exceeded");
    }

    attachment = GL_COLOR_ATTACHMENT0 + tex_ids.size(); // common attachment for color textures

    if (iformat == GL_RGBA16F || iformat == GL_RGBA32F) {
        format = GL_RGBA;
        type = GL_FLOAT;
    }
    else if (iformat == GL_RGB16F || iformat == GL_RGB32F) {
        format = GL_RGB;
        type = GL_FLOAT;
    }
    else if (iformat == GL_LUMINANCE16_ALPHA16) {
        format = GL_LUMINANCE_ALPHA;
        type = GL_FLOAT;
    }
    else if (iformat == GL_LUMINANCE16) {
        format = GL_LUMINANCE;
        type = GL_FLOAT;
    }
    else if (iformat == GL_RGBA8 || iformat == GL_RGBA || iformat == 4) {
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
    }
    else if (iformat == GL_RGB8 || iformat == GL_RGB || iformat == 3) {
        format = GL_RGB;
        type = GL_UNSIGNED_BYTE;
    }
    else if (iformat == GL_LUMINANCE8_ALPHA8 || iformat == GL_LUMINANCE_ALPHA || iformat == 2) {
        format = GL_LUMINANCE_ALPHA;
        type = GL_UNSIGNED_BYTE;
    }
    else if (iformat == GL_LUMINANCE8 || iformat == GL_LUMINANCE16 || iformat == GL_LUMINANCE || iformat == 1) {
        format = GL_LUMINANCE;
        type = GL_UNSIGNED_BYTE;
    }
    else if (iformat == GL_DEPTH_COMPONENT24 || iformat == GL_DEPTH_COMPONENT) {
        format = GL_DEPTH_COMPONENT;
        type = GL_UNSIGNED_INT;
        attachment = GL_DEPTH_ATTACHMENT;
        filter = GL_NEAREST;
    }
    else if (iformat == GL_STENCIL_INDEX1 || iformat == GL_STENCIL_INDEX4 || iformat == GL_STENCIL_INDEX8 ||
        iformat == GL_STENCIL_INDEX16 || iformat == GL_STENCIL_INDEX) {
        format = GL_STENCIL_INDEX;
        type = GL_UNSIGNED_BYTE;
        attachment = GL_STENCIL_ATTACHMENT;
        filter = GL_NEAREST;
    }
    else if (iformat == GL_DEPTH24_STENCIL8 || iformat == GL_DEPTH_STENCIL)
    {
        format = GL_DEPTH_STENCIL;
        type = GL_UNSIGNED_INT_24_8;
        attachment = GL_DEPTH_STENCIL_ATTACHMENT;
        filter = GL_NEAREST;
    }
    else if (iformat == GL_TEXTURE_2D_MULTISAMPLE)
    {
        format = GL_TEXTURE_2D_MULTISAMPLE;
        type = GL_RGB;
        attachment = GL_COLOR_ATTACHMENT0;
    }
    else {
        throw invalid_argument("FrameBuffer::attachTexture - unrecognized internal format");
    }

    glGenTextures(1, &tex_id);
    if (format == GL_TEXTURE_2D_MULTISAMPLE)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex_id);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGB, width, height, GL_TRUE);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, frame_id);
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glTexImage2D(GL_TEXTURE_2D, 0, iformat, width, height, 0, format, type, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    }

    if (format == GL_DEPTH_STENCIL)
    {
        // packed depth and stencil added separately
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex_id, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tex_id, 0);
    }
    else if (format == GL_TEXTURE_2D_MULTISAMPLE)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, tex_id, 0);
    }
    else
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex_id, 0);
    }

    tex_ids.push_back(tex_id);
    buffers[tex_ids.size() - 1] = attachment;
}

void FrameBuffer::bindInput()
{
    for (int i = 0; i < int(tex_ids.size()); i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, tex_ids[i]);
    }
}

void FrameBuffer::bindInput(int num) throw(out_of_range)
{
    if (num + 1 > int(tex_ids.size()))
    {
        throw out_of_range("FrameBuffer::bindInput - texture vector size exceeded");
    }
    glBindTexture(GL_TEXTURE_2D, tex_ids[num]);
}

void FrameBuffer::bindOutput() throw(domain_error)
{
    if (tex_ids.empty())
    {
        throw domain_error("FrameBuffer::bindOutput - no textures to bind");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, frame_id);
    if (tex_ids.size() == 1)
    {
        glDrawBuffer(buffers[0]);
    }
    else
    {
        glDrawBuffers(tex_ids.size(), buffers);
    }
}


void FrameBuffer::bindOutput(int num) throw(out_of_range)
{
    if (num + 1 > int(tex_ids.size()))
    {
        throw out_of_range("FrameBuffer::bindOutput - texture vector size exceeded");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, frame_id);
    glDrawBuffer(buffers[num]);
}

void FrameBuffer::bindTex(int num) throw(out_of_range)
{
    // Implemented through bindInput()
    bindInput(num);
}

// Bind image texture for compute processing
void FrameBuffer::bindImage(unsigned unit, int num, GLenum format, GLenum access) throw(std::out_of_range)
{
    if (num + 1 > int(tex_ids.size()))
    {
        throw out_of_range("FrameBuffer::bindWriteImage - texture vector size exceeded");
    }

    // Supported Formats:
    // GL_RGBA32F
    // GL_RGBA16F
    // GL_RGBA8
    // GL_RGBA8UI
    // GL_RGBA32I
    // others

    glBindImageTexture(unit, tex_ids[num], 0, GL_FALSE, 0, access, format);
}

// Bind the FBO for reading using GL_READ_FRAMEBUFFER
void FrameBuffer::bindRead()
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, frame_id);
}

// Bind the FBO for reading using GL_DRAW_FRAMEBUFFER
void FrameBuffer::bindWrite()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frame_id);
}

void FrameBuffer::check()
{
    GLenum status;

    glBindFramebuffer(GL_FRAMEBUFFER, frame_id);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        cout << "FBO Status error: " << status << endl;
        throw invalid_argument("FrameBuffer::Check - status error");
    }
}


void FrameBuffer::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
}