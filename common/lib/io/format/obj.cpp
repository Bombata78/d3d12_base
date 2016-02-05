///////////////////////////////////////////////////////////////////////////////////////////////////
//  obj.cpp
//    create mesh from obj file
///////////////////////////////////////////////////////////////////////////////////////////////////
#include <fstream>
#include <thread>
#include "glm/glm/glm.hpp"
#include "io/format/obj.h"
#include "core/utils.h"

namespace io
{

////////////////////////////////////////////////////////////////////////////////////////////////
/// OBJIterator class 
////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T>
class OBJVertexIterator{
public:
    OBJVertexIterator(const std::string& source, const std::string& delimiter);
    void next();
    std::vector<float>& current();

private:
    void findElement(size_t startPos);
    
    T                  source;
    std::string        delimiter;
    std::size_t        currentPos;
    std::vector<float> currentValue;
    std::string        element;
};

class OBJIndexIterator
{
public:
   OBJIndexIterator(const std::string& source);
   void next();
   const std::vector<std::uint32_t>& current();

private:
   void findElement(size_t startPos);

   const std::string&         source;
   std::size_t                currentPos;
   std::vector<std::uint32_t> currentValue;
   bool                       lineParsing;
   std::string                element;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// OBJParser class 
////////////////////////////////////////////////////////////////////////////////////////////////////
class OBJParser
{
public:
   OBJParser(const std::string& objFileName);

   std::unique_ptr<OBJVertexIterator<const std::string&>> getVertexCoordIterator() const 
      { return std::unique_ptr<OBJVertexIterator<const std::string&>>(new OBJVertexIterator<const std::string&>(source, "v")); }
   std::unique_ptr<OBJVertexIterator<const std::string>> getVertexCoordIteratorThreadSafe() const 
      { return std::unique_ptr<OBJVertexIterator<const std::string>>(new OBJVertexIterator<const std::string>(source, "v")); }
   std::unique_ptr<OBJVertexIterator<const std::string&>> getTextureCoordIterator() const 
      { return std::unique_ptr<OBJVertexIterator<const std::string&>>(new OBJVertexIterator<const std::string&>(source, "vt")); }
   std::unique_ptr<OBJVertexIterator<const std::string>> getTextureCoordIteratorThreadSafe() const 
      { return std::unique_ptr<OBJVertexIterator<const std::string>>(new OBJVertexIterator<const std::string>(source, "vt")); }
   std::unique_ptr<OBJVertexIterator<const std::string&>> getVertexNormalsIterator() const 
      { return std::unique_ptr<OBJVertexIterator<const std::string&>>(new OBJVertexIterator<const std::string&>(source, "vn")); }
   std::unique_ptr<OBJVertexIterator<const std::string>> getVertexNormalsIteratorThreadSafe() const 
      { return std::unique_ptr<OBJVertexIterator<const std::string>>(new OBJVertexIterator<const std::string>(source, "vn")); }
   std::unique_ptr<OBJIndexIterator> getIndicesIterator() const 
      { return std::unique_ptr<OBJIndexIterator>(new OBJIndexIterator(source)); }

private:
   std::string source;

};

////////////////////////////////////////////////////////////////////////////////////////////////////
// OBJVertexIterator::OBJVertexIterator
//    Constructor
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T>
OBJVertexIterator<T>::OBJVertexIterator(const std::string& source_, const std::string& delimiter_)
   : source(source_), delimiter(delimiter_ + " "), currentPos(0)
{
    currentValue.resize(3);
    element.resize(64);
    findElement(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// obj_vertex_iterator::next
//    Set the iterator to the next element
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T>
void OBJVertexIterator<T>::next()
{
   findElement((currentPos) ? currentPos + 1 : currentPos);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// obj_vertex_iterator::current
//    Return the current element
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T>
std::vector<float>& OBJVertexIterator<T>::current()
{
   return currentValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// obj_vertex_iterator::find_element
//    Set the iterator to the first element
////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T>
void OBJVertexIterator<T>::findElement(size_t startPos)
{
   // Find the first delimiter
   currentPos = startPos;
   do {
       currentPos = source.find(delimiter, currentPos);
   } while (currentPos != std::string::npos && currentPos != 0 && source.at(currentPos - 1) != ' ' && source.at(currentPos - 1) != '\n');

   try {
      if (currentPos == std::string::npos) throw std::exception();

      // Find first element
      element.clear();
      currentPos += 2;
      while (currentPos < source.length() && !isdigit(source.at(currentPos)) && source.at(currentPos) != '-' && source.at(currentPos) != '\n') currentPos++;
      if (currentPos == source.length() || source.at(currentPos) == '\n') throw std::exception();

      do {
         element += source.at(currentPos++);
      } while(currentPos < source.length() && (isdigit(source.at(currentPos)) || source.at(currentPos) == '.'));
      currentValue[0] = static_cast<float>(std::atof(element.c_str()));

      // Find second element
      while (currentPos < source.length() && !isdigit(source.at(currentPos)) && source.at(currentPos) != '-' && source.at(currentPos) != '\n') currentPos++;
      if (currentPos == source.length() || source.at(currentPos) == '\n') throw std::exception();
      element.clear();
      do {
         element += source.at(currentPos++);
      } while(currentPos < source.length() && (isdigit(source.at(currentPos)) || source.at(currentPos) == '.'));
      currentValue[1] = static_cast<float>(std::atof(element.c_str()));

      // Find third element
      while (currentPos < source.length() && !isdigit(source.at(currentPos)) && source.at(currentPos) != '-' && source.at(currentPos) != '\n') currentPos++;
      if (currentPos != source.length() && source.at(currentPos) != '\n') {
         element.clear();
         do {
            element += source.at(currentPos++);
         } while(currentPos < source.length() && (isdigit(source.at(currentPos)) || source.at(currentPos) == '.'));
         currentValue[2] = static_cast<float>(std::atof(element.c_str()));
      }
   }
   catch (std::exception&) {
       currentValue.clear();
   }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// OBJIndexIterator::OBJIndexIterator
//    Constructor
////////////////////////////////////////////////////////////////////////////////////////////////////
OBJIndexIterator::OBJIndexIterator(const std::string& source_)
   : source(source_), currentPos(0), lineParsing(false)
{
    currentValue.resize(3);
    element.resize(64);
    findElement(0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// OBJIndexIterator::next
//    Set the iterator to the next element
////////////////////////////////////////////////////////////////////////////////////////////////////
void OBJIndexIterator::next()
{
   findElement(currentPos);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// OBJIndexIterator::current
//    Return the current element
////////////////////////////////////////////////////////////////////////////////////////////////////
const std::vector<std::uint32_t>& OBJIndexIterator::current()
{
   return currentValue;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// OBJIndexIterator::find_element
//    Set the iterator to the first element
////////////////////////////////////////////////////////////////////////////////////////////////////
void OBJIndexIterator::findElement(size_t startPos)
{
   // Find the first delimiter
   currentPos = startPos;

   // Find the f
   if (!lineParsing) {
      do {
          currentPos = source.find("f ", currentPos);
      } while (currentPos != std::string::npos && currentPos != 0 && source.at(currentPos - 1) != ' ' && source.at(currentPos - 1) != '\n');
      if (currentPos != std::string::npos) currentPos++;
   }

   try {
      if (currentPos == std::string::npos || currentPos == source.length()) throw std::exception();

      // Find next element
      while (currentPos < source.length() && !isdigit(source.at(currentPos)) && source.at(currentPos) != '\n') currentPos++;
      if (currentPos == source.length() || source.at(currentPos) == '\n') throw std::exception();

      // Find vertex coords
      element.clear();
      do {
         element += source.at(currentPos++);
      } while(currentPos < source.length() && isdigit(source.at(currentPos)));
      // Validate position
      currentPos++;
      if (currentPos == source.length() || source.at(currentPos - 1) != '/') throw std::exception();
      currentValue[0] = std::atoi(element.c_str());

      // Find texture coords
      if (isdigit(source.at(currentPos))) {
         element.clear();
         do {
            element += source.at(currentPos++);
         } while(currentPos < source.length() && isdigit(source.at(currentPos)));
         currentValue[1] = std::atoi(element.c_str());
      }
      else if (source.at(currentPos) == '/')
         currentValue[1] = -1;
      // Validate position
      currentPos++;
      if (source.at(currentPos - 1) != '/') throw std::exception();

      // Find normal coords
      if (currentPos == source.length() || !isdigit(source.at(currentPos)))
         currentValue[2] = -1;
      else {
         element.clear();
         do {
            element += source.at(currentPos++);
         } while(currentPos < source.length() && isdigit(source.at(currentPos)));
         currentValue[2] = std::atoi(element.c_str());
      }

      while (currentPos < source.length() && !isdigit(source.at(currentPos)) && source.at(currentPos) != '\n') currentPos++;
      lineParsing = (currentPos != source.length() && source.at(currentPos) != '\n') ;

   }
   catch (std::exception&) {
       currentValue.clear();
   }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// OBJParser::OBJParser
//    
////////////////////////////////////////////////////////////////////////////////////////////////////
OBJParser::OBJParser(const std::string& objFileName)
{
    // Load the file
    std::ifstream file(objFileName);
    file.seekg(0, std::ios::end);
    std::size_t fileSize = file.tellg();
    source.resize(fileSize);
    file.seekg(0, std::ios::beg);
    file.read(&source[0], fileSize);
    file.close();

    // Load the file
    //std::ifstream file(objFileName);
    //if (!file.is_open()) throw std::exception("Can't open obj file.");
    //
    //// Allocate the string pointer and copy the content of the file to it
    //source = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

#define USE_MULTITHREAD_MESH_LOADING 1
mesh* create_mesh_from_obj(const std::string& filename, bool invert_uvs)
{
    std::vector<float>         objVertices;
    std::vector<float>         objTexCoord;
    std::vector<float>         objNormals;
    std::vector<std::uint32_t> objIndices;

    // Load the obj file data
    {
        // Pre-allocate data
        {
            std::streampos fileSize = 0;
            std::ifstream file(filename, std::ios::binary);
            if (!file.is_open()) throw std::exception("Can't open obj");
            fileSize = file.tellg();
            file.seekg( 0, std::ios::end );
            fileSize = file.tellg() - fileSize;
            file.close();

            objVertices.reserve(fileSize/4);
            objTexCoord.reserve(fileSize/4);
            objNormals.reserve(fileSize/4);
            objIndices.reserve(fileSize/4);
        }

        OBJParser parser(filename.c_str());

#ifdef USE_MULTITHREAD_MESH_LOADING
        std::unique_ptr<OBJVertexIterator<const std::string>> vertexCoordIt   = parser.getVertexCoordIteratorThreadSafe();
        std::unique_ptr<OBJVertexIterator<const std::string>> textureCoordIt  = parser.getTextureCoordIteratorThreadSafe();
        std::unique_ptr<OBJVertexIterator<const std::string>> vertexNormalsIt = parser.getVertexNormalsIteratorThreadSafe();

        auto pfnThread = [](std::vector<float>* pObjData, std::uint32_t coordSize, OBJVertexIterator<const std::string>* it)
        {
            for (; it->current().size(); it->next())
            {
                for (std::uint32_t i = 0; i < coordSize; ++i)
                {
                    pObjData->push_back(it->current()[i]);
                }
            }
        };
        std::thread threadVertex(pfnThread, &objVertices, 3, vertexCoordIt.get());
        std::thread threadTexCoord(pfnThread, &objTexCoord, 2, textureCoordIt.get());
        std::thread threadNormal(pfnThread, &objNormals, 3, vertexNormalsIt.get());
#else
        std::unique_ptr<OBJVertexIterator<const std::string&>> vertexCoordIt   = parser.getVertexCoordIterator();
        std::unique_ptr<OBJVertexIterator<const std::string&>> textureCoordIt  = parser.getTextureCoordIterator();
        std::unique_ptr<OBJVertexIterator<const std::string&>> vertexNormalsIt = parser.getVertexNormalsIterator();

        // Build the vertex data
        for (; vertexCoordIt->current().size(); vertexCoordIt->next())
        {
            objVertices.push_back(vertexCoordIt->current()[0]);
            objVertices.push_back(vertexCoordIt->current()[1]);
            objVertices.push_back(vertexCoordIt->current()[2]);
        }
        // Build the texture coord data
        for (; textureCoordIt->current().size(); textureCoordIt->next())
        {
            objTexCoord.push_back(textureCoordIt->current()[0]);
            objTexCoord.push_back(textureCoordIt->current()[1]);
        }
        // Build the vertex normal data
        for (; vertexNormalsIt->current().size(); vertexNormalsIt->next())
        {
            objNormals.push_back(vertexNormalsIt->current()[0]);
            objNormals.push_back(vertexNormalsIt->current()[1]);
            objNormals.push_back(vertexNormalsIt->current()[2]);
        }
#endif

        // Build the index data
        std::unique_ptr<OBJIndexIterator> indicesIt = parser.getIndicesIterator();
        for (;indicesIt->current().size(); indicesIt->next())
        {
            objIndices.push_back(indicesIt->current()[0]);
            objIndices.push_back(indicesIt->current()[1]);
            objIndices.push_back(indicesIt->current()[2]);
        }

#if USE_MULTITHREAD_MESH_LOADING
        // Wait for thread completion
        threadVertex.join();
        threadTexCoord.join();
        threadNormal.join();
#endif

        indicesIt.reset();
        vertexNormalsIt.reset();
        textureCoordIt.reset();
        vertexCoordIt.reset();
    }

    // Build the vertex attributes
    std::vector<mesh::attribute> attributes;
    attributes.push_back(mesh::attribute(mesh::attribute::position, format::r32g32b32_float, 0));
    attributes.push_back(mesh::attribute(mesh::attribute::texcoord, format::r32g32_float,    get_stride_from_format(format::r32g32b32_float)));
    attributes.push_back(mesh::attribute(mesh::attribute::normal,   format::r32g32b32_float, attributes[attributes.size() - 1].offset + get_stride_from_format(format::r32g32_float)));

    // Allocate the vertex/index buffers
    std::size_t vertexByteStride = attributes[2].offset + get_stride_from_format(format::r32g32b32_float);
    io::buffer    vertexBuffer(vertexByteStride, objIndices.size()/3 * vertexByteStride);
    ::format indexFormat = (objIndices.size()/3 <= 0xFFFF) ? ::format::r16_uint : ::format::r32_uint;
    io::buffer    indexBuffer(indexFormat, objIndices.size()/3 * get_stride_from_format(indexFormat));

    // Fill the vertex/index buffer
    std::uint8_t* pVertexData = reinterpret_cast<std::uint8_t*>(vertexBuffer.data());
    std::uint8_t* pIndexData  = reinterpret_cast<std::uint8_t*>(indexBuffer.data());
    for (std::uint32_t i = 0; i < objIndices.size(); i+=3)
    {
        reinterpret_cast<glm::vec3*>(pVertexData)->x = objVertices[(objIndices[i]-1) * 3];
        reinterpret_cast<glm::vec3*>(pVertexData)->y = objVertices[(objIndices[i]-1) * 3 + 1];
        reinterpret_cast<glm::vec3*>(pVertexData)->z = objVertices[(objIndices[i]-1) * 3 + 2];
        pVertexData += get_stride_from_format(format::r32g32b32_float);

        // Fill texture coordinates
        if (objTexCoord.size())
        {
            reinterpret_cast<glm::vec2*>(pVertexData)->x = objTexCoord[(objIndices[i+1]-1) * 2];
            reinterpret_cast<glm::vec2*>(pVertexData)->y = objTexCoord[(objIndices[i+1]-1) * 2 + 1];
            if (invert_uvs)
            {
                //float v = reinterpret_cast<glm::vec2*>(pVertexData)->y;
                //float integer = std::trunc(v);
                //float decimal = v - integer;
                //if (decimal >= 0) decimal =  1.0f - decimal;
                //else              decimal = -1.0f + decimal;  
                //reinterpret_cast<glm::vec2*>(pVertexData)->y = integer + decimal;
                reinterpret_cast<glm::vec2*>(pVertexData)->y = 1.0f - reinterpret_cast<glm::vec2*>(pVertexData)->y;
            }
        }
        else
        {
            reinterpret_cast<glm::vec2*>(pVertexData)->x = 0.0f;
            reinterpret_cast<glm::vec2*>(pVertexData)->y = 0.0f;
        }
        pVertexData += get_stride_from_format(format::r32g32_float);

        // Fill normal vector
        reinterpret_cast<glm::vec3*>(pVertexData)->x = objNormals[(objIndices[i+2]-1)*3];
        reinterpret_cast<glm::vec3*>(pVertexData)->y = objNormals[(objIndices[i+2]-1)*3 + 1];
        reinterpret_cast<glm::vec3*>(pVertexData)->z = objNormals[(objIndices[i+2]-1)*3 + 2];
        pVertexData += get_stride_from_format(format::r32g32b32_float);

        // Fill index
        if (indexFormat == ::format::r16_uint)
        {
            *reinterpret_cast<std::uint16_t*>(pIndexData) = static_cast<std::uint16_t>(i/3);
        }
        else
        {
            *reinterpret_cast<std::uint32_t*>(pIndexData) = i/3;
        }
        pIndexData += get_stride_from_format(indexFormat);
    }

    return new mesh(topology::triangle_list, std::move(vertexBuffer), std::move(indexBuffer), std::move(attributes));
}

};