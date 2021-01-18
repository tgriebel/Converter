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
static const std::string ConvertedPath = "models/";

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


void LoadImage( const std::string& path, Image<Color>& outImage )
{
    int32_t width;
    int32_t height;
    int32_t channels;
    stbi_uc* pixels = stbi_load( path.c_str(), &width, &height, &channels, STBI_rgb_alpha );
    int32_t imageSize = width * height * 4;

    if ( !pixels )
    {
        throw std::runtime_error( "Failed to load texture image!" );
    }

    outImage = Image<Color>( width, height );
    for ( int32_t y = 0; y < height; ++y )
    {
        for ( int32_t x = 0; x < width; ++x )
        {
            Pixel pixel;
            pixel.rgba.r = pixels[ ( y * width + x ) * 4 + 0 ];
            pixel.rgba.g = pixels[ ( y * width + x ) * 4 + 1 ];
            pixel.rgba.b = pixels[ ( y * width + x ) * 4 + 2 ];
            pixel.rgba.a = pixels[ ( y * width + x ) * 4 + 3 ];

            outImage.SetPixel( x, y, Color( pixel.r8g8b8a8 ) );
        }
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

    using indexBuffer = std::vector<uint32_t>;

    std::vector<vertex_t>       vertices;
    std::vector<indexBuffer>    indexBuffers;
    std::vector<vertex_t>       uniqueVertices{};

    const uint32_t shapeCount = shapes.size();
    const uint32_t vertexCount = attrib.vertices.size();
    const uint32_t normalCount = attrib.normals.size();
    const uint32_t textureCount = attrib.texcoords.size();

    indexBuffers.resize( shapeCount );
    
    for ( uint32_t shapeIx = 0; shapeIx < shapeCount; ++shapeIx )
    {
        indexBuffer& indices = indexBuffers[ shapeIx ];

        tinyobj::shape_t& shape = shapes[ shapeIx ];

        for ( const auto& index : shape.mesh.indices )
        {
            vertex_t vert;

            if( ( index.vertex_index >= 0 ) && ( ( 3 * index.vertex_index + 2 ) < vertexCount ) )
            {
                vert.pos[ 0 ] = attrib.vertices[ 3 * index.vertex_index + 0 ];
                vert.pos[ 1 ] = attrib.vertices[ 3 * index.vertex_index + 1 ],
                vert.pos[ 2 ] = attrib.vertices[ 3 * index.vertex_index + 2 ];
            }
            else
            {
                vert.pos = vec4d( 0.0, 0.0, 0.0 );
            }

            if ( ( index.normal_index >= 0 ) && ( ( 3 * index.normal_index + 2 ) < normalCount ) )
            {
                vert.normal[ 0 ] = attrib.normals[ 3 * index.normal_index + 0 ];
                vert.normal[ 1 ] = attrib.normals[ 3 * index.normal_index + 1 ],
                vert.normal[ 2 ] = attrib.normals[ 3 * index.normal_index + 2 ];
            }
            else
            {
                vert.normal = vec3d( 1.0, 0.0, 0.0 ).Normalize();
            }

            if ( ( index.texcoord_index >= 0 ) && ( ( 2 * index.texcoord_index + 1 ) < textureCount ) )
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

    const uint32_t materialCount = materials.size();
    for ( uint32_t i = 0; i < materialCount; ++i )
    {
        tinyobj::material_t& material = materials[ i ];
        material_t m;

        memset( m.name, 0, material_t::BufferSize );
        strcpy_s( m.name, material_t::BufferSize, material.name.c_str() );
        m.Ni = material.ior;
        m.Ns = material.shininess;
        m.Ka = material.ambient[ 0 ];
        m.Ke = material.emission[ 0 ];
        m.Kd = material.diffuse[ 0 ];
        m.Ks = material.specular[ 0 ];
        m.Tf = material.transmittance[ 0 ];
        m.Tr = 1.0 - Saturate( material.dissolve );
        m.d = material.dissolve;
        m.illum = material.illum;
        m.textured = false;  

        std::string name = path.substr( path.find_last_of( '/' ) + 1, path.size() );

        if ( material.diffuse_texname.size() > 0 )
        {
            Image<Color> image;
            LoadImage( std::string( TexturePath + material.diffuse_texname ), image );

            Bitmap bitmap = Bitmap( image.GetWidth(), image.GetHeight() );
            ImageToBitmap( image, bitmap );
            bitmap.Write( "testConvert.bmp" );

            m.colorMapId = rm.StoreImageCopy( image );
            m.textured = true;
        }

        rm.StoreMaterialCopy( m );
    }

    // Build VB and IB
    // VB is shared while IB is shared but partitioned by shape
    {
        model->surfs.resize( shapeCount );

        uint32_t vbOffset = rm.GetVbOffset();
        const uint32_t vertexCnt = uniqueVertices.size();
        for ( int32_t i = 0; i < vertexCnt; ++i )
        {
            rm.AddVertex( uniqueVertices[ i ] );
        }
        uint32_t vbEnd = rm.GetVbOffset();

        for ( uint32_t shapeIx = 0; shapeIx < shapeCount; ++shapeIx )
        {
            model->name = path;

            surface_t& surf = model->surfs[ shapeIx ];

            surf.vb = rm.GetVB();
            surf.ib = rm.GetIB();       
            surf.vbOffset = vbOffset;
            surf.vbEnd = vbEnd;

            surf.ibOffset = rm.GetIbOffset();
            const size_t indexCnt = indexBuffers[ shapeIx ].size();
            assert( ( indexCnt % 3 ) == 0 );
            for ( size_t i = 0; i < indexCnt; i++ )
            {
                rm.AddIndex( surf.vbOffset + indexBuffers[ shapeIx ][ i ] );
            }
            surf.ibEnd = rm.GetIbOffset();

            // Intentionally does not support per-vertex materials
            if( shapes[ shapeIx ].mesh.material_ids.size() > 0 )
            {
                surf.materialId = shapes[ shapeIx ].mesh.material_ids[ 0 ];
            }
            else
            {
                surf.materialId = 0;
            }
        }
    }

    return modelIx;
}


int main()
{
    //std::vector<std::string> models = { "12140_Skull_v3_L2", "sphere", "box" };

    std::vector<std::string> models = { "legoToys" };

    for( uint32_t i = 0; i < models.size(); ++i )
    {
        std::cout << "Converting: " << models[ i ] << "...\n";

        ResourceManager modelRM;

        uint32_t vb = modelRM.AllocVB();
        uint32_t ib = modelRM.AllocIB();

        modelRM.PushVB( vb );
        modelRM.PushIB( ib );

        std::string& modelName = models[ i ];

        uint32_t srcModelId = LoadModel( ModelPath + modelName + ".obj", modelRM );

        StoreModelBin( ConvertedPath + modelName + ".mdl", modelRM, srcModelId );
        uint32_t modelIx = LoadModelBin( ConvertedPath + modelName + ".mdl", modelRM );
        // StoreModelObj( "test.obj", modelRM, modelIx );
    }
}