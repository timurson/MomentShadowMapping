#ifndef _FRAME_BUFFER_H_
#define _FRAME_BUFFER_H_

#include <glad/glad.h> // holds all OpenGL type declarations
#include <stdexcept>
#include <vector>

// Frame Buffer Object class
class FrameBuffer
{
public:
    // default constructor
    FrameBuffer();
    // size constructor
    FrameBuffer(int width, int height);
    // destructor
    ~FrameBuffer();
    // Set FBO size when using default constructor
    void setSize(int width_, int height_) { width = width_; height = height_; }
    // Attach a render target to the FBO
    void attachRender(GLenum iformat, bool multisample = false) throw(std::domain_error, std::invalid_argument);
    // Attach a texture to the FBO
    void attachTexture(GLenum iformat, GLint filter = GL_LINEAR) throw(std::domain_error, std::out_of_range, std::invalid_argument);
    // Bind the FBO as input, for reading from
    void bindInput();
    // Bind the nth texture of the FBO as input
    void bindInput(int num) throw(std::out_of_range);
    // Bind the FBO as output, for writing into
    void bindOutput() throw(std::domain_error);
    // Bind the nth texture of the FBO as output
    void bindOutput(int num) throw(std::out_of_range);
    // Bind the specified FBO texture to the context (deprecated)
    void bindTex(int num = 0) throw(std::out_of_range);
    // Bind image texture for compute read/writing
    void bindImage(unsigned unit, int num, GLenum format, GLenum access = GL_READ_WRITE) throw(std::out_of_range);
    // Bind the FBO for reading using GL_READ_FRAMEBUFFER
    void bindRead();
    // Bind the FBO for reading using GL_DRAW_FRAMEBUFFER
    void bindWrite();
    // Check OpenGL status of the FBO
    void check();
    // Disable rendering to FBO
    static void unbind();

private:
    int max_color_attachments;    // maximum number of color attachments allowed
    int width;                    // width of this RT
    int height;                   // height of this RT
    GLenum * buffers;             // glDrawBuffers ()
    GLuint frame_id;              // frame buffer object id
    GLuint depth_id;              // depth render buffer id
    GLuint stencil_id;            // stencil render buffer id
    std::vector<GLuint> tex_ids;  // ids of render target textures

};


#endif
