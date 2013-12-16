#ifndef MATERIAL_H
#define MATERIAL_H

#include <QVector4D>
#include "../helpers/opengluniformbuffer.h"

#include <vector>

class Material
{

public:
    Material(const string& name,
             const QVector4D& ambientColor,
             const QVector4D& diffuseColor,
             const QVector4D& specularColor,
             const QVector4D& emissiveColor,
             float shininess,
             float shininessStrength,
              bool hasTexture,
               int twoSided,
             GLuint programHandle);

    virtual ~Material();

    void setName(const string& name) { m_name = name; }
    string name() const { return m_name; }

    void bind();

private:
    void init();
    void fillBuffer(vector<GLubyte>& buffer, GLint* offsets);

    string m_name;

    QVector4D m_ambientColor;
    QVector4D m_diffuseColor;
    QVector4D m_specularColor;
    QVector4D m_emissiveColor;

    float m_shininess;
    float m_shininessStrength;

    bool m_hasTexture;
    int  m_twoSided;

    OpenGLUniformBuffer m_uniformsBuffer;

};

#endif // MATERIAL_H
