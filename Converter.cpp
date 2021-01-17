#include <iostream>
#include <vector>
#include <assert.h>
#include "../GfxCore/color.h"
#include "../GfxCore/image.h"
#include "../GfxCore/geom.h"
#include "../GfxCore/resourceManager.h"
#include "../GfxCore/util.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

static const std::string ModelPath = "models/";
static const std::string TexturePath = "textures/";
static const std::string ConvertedPath = "converted/";

ResourceManager rm;

enum imageFormat_t
{
    IMAGE_FORMAT_BMP,
    IMAGE_FORMAT_PNG,
    IMAGE_FORMAT_BIN,
};

void ConvertImage( const std::string& srcFileName, const std::string& dstFileName, imageFormat_t format )
{
    int32_t width;
    int32_t height;
    int32_t channels;
    stbi_uc* pixels = stbi_load( ( TexturePath + srcFileName ).c_str(), &width, &height, &channels, STBI_rgb_alpha );
    int32_t imageSize = width * height * 4;

    if ( !pixels )
    {
        throw std::runtime_error( "Failed to load texture image!" );
    }

    if( format == IMAGE_FORMAT_BMP )
    {
        stbi_write_bmp( ( dstFileName + ".bmp" ).c_str(), width, height, channels, pixels );
    }
    else if( format == IMAGE_FORMAT_PNG )
    {
        stbi_write_png( ( dstFileName + ".png" ).c_str(), width, height, channels, pixels, width * channels );
    }
    else if ( format == IMAGE_FORMAT_BIN )
    {
        
    }
    stbi_image_free( pixels );
}

uint32_t LoadModel( const std::string& path, ResourceManager& rm )
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if ( !tinyobj::LoadObj( &attrib, &shapes, &materials, &warn, &err, path.c_str(), ModelPath.c_str() ) )
    {
        throw std::runtime_error( warn + err );
    }

    std::string materialName = "";
    if( materials.size() )
    {
        materialName = materials[ 0 ].name;
    }

    std::vector<vertex_t> vertices;
    std::vector<uint32_t> indices;
    std::vector<vertex_t> uniqueVertices{};

    for ( const auto& shape : shapes ) {
        for ( const auto& index : shape.mesh.indices )
        {
            vertex_t vert;

            vert.pos[ 0 ] = attrib.vertices[ 3 * index.vertex_index + 0 ];
            vert.pos[ 1 ] = attrib.vertices[ 3 * index.vertex_index + 1 ],
            vert.pos[ 2 ] = attrib.vertices[ 3 * index.vertex_index + 2 ];

            if ( index.normal_index != -1 )
            {
                vert.normal[ 0 ] = attrib.vertices[ 3 * index.normal_index + 0 ];
                vert.normal[ 1 ] = attrib.vertices[ 3 * index.normal_index + 1 ],
                vert.normal[ 2 ] = attrib.vertices[ 3 * index.normal_index + 2 ];
            }
            else
            {
                vert.normal = vec3d( 1.0, 0.0, 0.0 ).Normalize();
            }

            if ( index.texcoord_index != -1 )
            {
                vert.uv[ 0 ] = attrib.texcoords[ 2 * index.texcoord_index + 0 ];
                vert.uv[ 1 ] = attrib.texcoords[ 2 * index.texcoord_index + 1 ];
            }
            else
            {
                vert.uv = vec2d( 0.0, 0.0 );
            }

            vert.color = Color::White;

            auto it = std::find( uniqueVertices.begin(), uniqueVertices.end(), vert );

            if ( it == uniqueVertices.end() )
            {
                indices.push_back( uniqueVertices.size() );
                uniqueVertices.push_back( vert );      
            }
            else
            {
                indices.push_back( std::distance( uniqueVertices.begin(), it ) );
            }

            /*
            if ( shape.mesh.num_face_vertices == 4 )
            {
                // For quads do a very simple triangulation by appending the two
                // adjacent indices to make the second triangle in the quad.
                // This totals 6 indices, 3 for each triangle
                // TODO: winding order
                assert( indices.size() >= 4 );
                const uint32_t v0 = *( indices.end() - 4 );
                const uint32_t v2 = *( indices.end() - 2 );

                indices.push_back( v0 );
                indices.push_back( v2 );
            }
            */
        }
    }

    // Normalize UVs to [0, 1]
    // TODO: leave as-is and let texture wrap mode deal with it?
    {
        auto vertEnd = uniqueVertices.end();
        for ( auto it = uniqueVertices.begin(); it != vertEnd; ++it )
        {
            it->uv[ 0 ] = ( it->uv[ 0 ] > 1.0 ) ? ( it->uv[ 0 ] - floor( it->uv[ 0 ] ) ) : it->uv[ 0 ];
            it->uv[ 1 ] = ( it->uv[ 1 ] > 1.0 ) ? ( it->uv[ 1 ] - floor( it->uv[ 1 ] ) ) : it->uv[ 1 ];

            it->uv[ 0 ] = Saturate( it->uv[ 0 ] );
            it->uv[ 1 ] = Saturate( it->uv[ 1 ] );

            it->uv[ 1 ] = 1.0 - it->uv[ 1 ];
        }
    }

    /////////////////////////////////////////////
    //                                         //
    // Construct final object representation   //
    //                                         //
    /////////////////////////////////////////////

    const uint32_t modelIx = rm.AllocModel();
    Model* model = rm.GetModel( modelIx );

    // Build VB and IB
    {
        model->name = path;
        model->vb = rm.GetVB();
        model->ib = rm.GetIB();
        model->vbOffset = rm.GetVbOffset();
        model->ibOffset = rm.GetIbOffset();

        const uint32_t vertexCnt = uniqueVertices.size();
        for ( int32_t i = 0; i < vertexCnt; ++i )
        {
            rm.AddVertex( uniqueVertices[ i ] );
        }
        model->vbEnd = rm.GetVbOffset();

        const size_t indexCnt = indices.size();
        assert( ( indexCnt % 3 ) == 0 );
        for ( size_t i = 0; i < indexCnt; i++ )
        {
            rm.AddIndex( model->vbOffset + indices[ i ] );
        }
        model->ibEnd = rm.GetIbOffset();
    }

    std::string basePath = path.substr( 0, path.find_last_of( '/' ) );

    LoadMaterialObj( basePath + "/" + materialName, rm, model->material );


    return modelIx;
}


int main()
{ 
//  LoadModel( "2-Gears-LOW.obj" );
//  ConvertImage( "EngineA_diff.png", "EngineA_diff", IMAGE_FORMAT_PNG );

    uint32_t vb = rm.AllocVB();
    uint32_t ib = rm.AllocIB();

    rm.PushVB( vb );
    rm.PushIB( ib );

    std::string modelName = "2-Gears-LOW";

    uint32_t srcModelId = LoadModel( ModelPath + modelName + ".obj", rm );

    StoreModelBin( ConvertedPath + modelName + ".mdl", rm, srcModelId );
    uint32_t modelIx = LoadModelBin( ConvertedPath + modelName + ".mdl", rm );

    Model* model = rm.GetModel( modelIx );
    std::string basePath = ModelPath.substr( 0, ModelPath.find_last_of( '/' ) );
    LoadMaterialObj( basePath + "/" + model->material.name, rm, model->material );

    StoreModelObj( "test.obj", rm, modelIx );
}