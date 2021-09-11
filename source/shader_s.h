#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <string>
#include <iostream>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Shader
{
public:
    unsigned int ID;
    // constructor generates the shader on the fly
    Shader(const char* vShaderSource, const char* fShaderSource, const char* gShaderSource = nullptr)
    {
        // compile shaders
        unsigned int vertex, fragment;
        // vertex shader
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderSource, NULL);
        glCompileShader(vertex);
        if (!checkCompileErrors(vertex, "VERTEX"))
        {
            std::cout << vShaderSource << std::endl;
        }
        // fragment Shader
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderSource, NULL);
        glCompileShader(fragment);
        if (!checkCompileErrors(fragment, "FRAGMENT"))
        {
            std::cout << fShaderSource << std::endl;
        }
        // if the geometry shader is given, compile geometry shader
        unsigned int geometry;
        if (gShaderSource != nullptr)
        {
            geometry = glCreateShader(GL_GEOMETRY_SHADER);
            glShaderSource(geometry, 1, &gShaderSource, NULL);
            glCompileShader(geometry);
            if (!checkCompileErrors(fragment, "GEOMETRY"))
            {
                std::cout << gShaderSource << std::endl;
            }

        }
        // shader Program
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        if (gShaderSource != nullptr) {
            glAttachShader(ID, geometry);
        }
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");
        // delete the shaders as they're linked into our program now and no longer necessary
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        if (gShaderSource != nullptr) {
            glDeleteShader(geometry);
        }      
    }

    // constructor for compute shader
    Shader(const char* cShaderSource)
    {
        unsigned int compute = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(compute, 1, &cShaderSource, NULL);
        glCompileShader(compute);
        if (!checkCompileErrors(compute, "COMPUTE"))
        {
            std::cout << cShaderSource << std::endl;
        }

        // shader Program
        ID = glCreateProgram();
        glAttachShader(ID, compute);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");
        glDeleteShader(compute);
    }


    // activate the shader
    // ------------------------------------------------------------------------
    void use()
    {
        glUseProgram(ID);
    }
    // utility uniform functions
    void setUniformBool(const std::string &uniformName, bool value) const
    {
        glUniform1i(glGetUniformLocation(ID, uniformName.c_str()), (int)value);
    }
    // ------------------------------------------------------------------------
    void setUniformInt(const std::string &uniformName, int value) const
    {
        glUniform1i(glGetUniformLocation(ID, uniformName.c_str()), value);
    }
    // ------------------------------------------------------------------------
    void setUniformFloat(const std::string &uniformName, float value) const
    {
        glUniform1f(glGetUniformLocation(ID, uniformName.c_str()), value);
    }
    // ------------------------------------------------------------------------
    void setUniformVec2f(const std::string &uniformName, glm::vec2& value) const
    {
        glUniform2f(glGetUniformLocation(ID, uniformName.c_str()), value.x, value.y);
    }
    void setUniformVec2f(const std::string &uniformName, float x, float y) const
    {
        glUniform2f(glGetUniformLocation(ID, uniformName.c_str()), x, y);
    }
    // ------------------------------------------------------------------------
    void setUniformVec2fv(const std::string &uniformName, const float* floats) const
    {
        glUniform2fv(glGetUniformLocation(ID, uniformName.c_str()), 1, floats);
    }
    // ------------------------------------------------------------------------
    void setUniformVec3f(const std::string &uniformName, glm::vec3& value) const
    {
        glUniform3f(glGetUniformLocation(ID, uniformName.c_str()), value.x, value.y, value.z);
    }
    void setUniformVec3f(const std::string &uniformName, float x, float y, float z) const
    {
        glUniform3f(glGetUniformLocation(ID, uniformName.c_str()), x, y, z);
    }
    // ------------------------------------------------------------------------
    void setUniformVec3fv(const std::string &uniformName, const float* floats) const
    {
        glUniform3fv(glGetUniformLocation(ID, uniformName.c_str()), 1, floats);
    }
    // ------------------------------------------------------------------------
    void setUniformVec4f(const std::string &uniformName, glm::vec4& value) const
    {
        glUniform4f(glGetUniformLocation(ID, uniformName.c_str()), value.x, value.y, value.z, value.a);
    }
    // ------------------------------------------------------------------------
    void setUniformVec4fv(const std::string &uniformName, const float* floats) const
    {
        glUniform4fv(glGetUniformLocation(ID, uniformName.c_str()), 1, floats);
    }
    // ------------------------------------------------------------------------
    void setUniformMat4(const std::string &uniformName, const glm::mat4 &matrix) const
    {
        glUniformMatrix4fv(glGetUniformLocation(ID, uniformName.c_str()), 1, GL_FALSE, &matrix[0][0]);
    }


private:
    // utility function for checking shader compilation/linking errors.
    // ------------------------------------------------------------------------
    int checkCompileErrors(unsigned int shader, std::string type)
    {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM")
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success)
            {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success)
            {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
        return success;
    }
};


#endif
