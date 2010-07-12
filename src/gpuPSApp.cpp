#include "cinder/app/AppBasic.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/Vbo.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/ArcBall.h"
#include "cinder/Camera.h"
#include "cinder/ImageIo.h"
#include "cinder/Vector.h"
#include "cinder/Rect.h"
#include "cinder/Rand.h"
#include "cinder/Utilities.h"

#define SIDE 600

using namespace ci;
using namespace ci::app;
using namespace std;

class gpuPSApp : public AppBasic {
public:
	void	prepareSettings( Settings *settings );
	void	setup();
	void	resize( int width, int height );
	void    mouseDown( MouseEvent event );
	void    mouseDrag( MouseEvent event );
	void	draw();
	CameraPersp		mCam;
	Arcball			mArcball;
	Surface32f		mInitPos;
	gl::Fbo			mFBO;
	gl::Texture		mTexture;
	gl::VboMesh		mVboMesh;
	gl::GlslProg	mShader;
};

void gpuPSApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( 1000, 800 );
	//settings->setFullScreen(true);
}

void gpuPSApp::setup()
{	
	mArcball.setQuat( Quatf( Vec3f( 0.0577576f, -0.956794f, 0.284971f ), 3.68f ) );
	
	//====== Shader ======
	try {
		mShader = gl::GlslProg( loadResource( "vDispl.vert" ), loadResource( "vDispl.frag" ));
	}
	catch( ci::gl::GlslProgCompileExc &exc ) {
		std::cout << "Shader compile error: " << endl;
		std::cout << exc.what();
	}
	catch( ... ) {
		std::cout << "Unable to load shader" << endl;
	}
	
	float ang;
	//====== Texture ======
	mInitPos = Surface32f( SIDE, SIDE, true);
	Surface32f::Iter pixelIter = mInitPos.getIter();
	while( pixelIter.line() ) {
		while( pixelIter.pixel() ) {
			// Initial particle positions within the RBG color values.
			ang += Rand::randFloat(0.525f,0.530f);
			mInitPos.setPixel( pixelIter.getPos(), ColorAf( cosf(ang), 0.0f, sinf(ang), 1.0f ) );
		}
	}
	gl::Texture::Format tFormat;
	tFormat.setInternalFormat(GL_RGBA16F_ARB);
	mTexture = gl::Texture( mInitPos, tFormat);
	mTexture.setWrap( GL_REPEAT, GL_REPEAT );
	mTexture.setMinFilter( GL_NEAREST );
	mTexture.setMagFilter( GL_NEAREST );
	
	//======  FBO ======
	gl::Fbo::Format format;
	format.enableDepthBuffer(false);
	format.enableColorBuffer(true);
	format.setColorInternalFormat( GL_RGBA32F_ARB );
	mFBO = gl::Fbo( SIDE, SIDE, format );
	
	mFBO.bindFramebuffer();
	
	gl::setMatricesWindow( mFBO.getSize(), false );
	gl::setViewport( mFBO.getBounds() );
	
	mTexture.enableAndBind();
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	
	gl::drawSolidRect( mFBO.getBounds() );
	mTexture.unbind();
	mFBO.unbindFramebuffer();
	//console()<<SIDE*SIDE<<" vertices!!!"<<std::endl;
	
	
	//====== VboMesh the same size as the texture ======
	/* THE VBO HAS TO BE DRAWN AFTER FBO!
	 http://www.openframeworks.cc/forum/viewtopic.php?f=9&t=2443
	 */
	int totalVertices = SIDE * SIDE;
	int totalQuads = ( SIDE - 1 ) * ( SIDE - 1 );
	vector<Vec2f> texCoords;
	vector<Vec3f> vCoords, normCoords;
	vector<uint32_t> indices;
	gl::VboMesh::Layout layout;
	layout.setStaticIndices();
	layout.setStaticPositions();
	layout.setStaticTexCoords2d();
	layout.setStaticNormals();
	mVboMesh = gl::VboMesh( totalVertices, totalQuads * 4, layout, GL_POINTS); // GL_LINES GL_QUADS
	for( int x = 0; x < SIDE; ++x ) {
		for( int y = 0; y < SIDE; ++y ) {	
			if( ( x + 1 < SIDE ) && ( y + 1 < SIDE ) ) {
				indices.push_back( (x+0) * SIDE + (y+0) );
				indices.push_back( (x+1) * SIDE + (y+0) );
				indices.push_back( (x+1) * SIDE + (y+1) );
				indices.push_back( (x+0) * SIDE + (y+1) );
			}
			texCoords.push_back( Vec2f( x/(float)SIDE, y/(float)SIDE ) );
			vCoords.push_back( Vec3f( x/(float)SIDE, 0, y/(float)SIDE ) );
		}
	}
	mVboMesh.bufferIndices( indices );
	mVboMesh.bufferTexCoords2d( 0, texCoords );
	mVboMesh.bufferPositions( vCoords );
	
	gl::enableDepthRead();
	glColor3f(1.0f, 1.0f, 1.0f);
}

void gpuPSApp::resize( int width, int height )
{
	mArcball.setWindowSize( getWindowSize() );
	mArcball.setCenter( Vec2f( getWindowWidth() / 2.0f, getWindowHeight() / 2.0f ) );
	mArcball.setRadius( getWindowHeight() / 2.0f );
	
	mCam.lookAt( Vec3f( 0.0f, 0.0f, -450.0f ), Vec3f::zero() );
	mCam.setPerspective( 60.0f, getWindowAspectRatio(), 0.1f, 2000.0f );
	gl::setMatrices( mCam );
}

void gpuPSApp::mouseDown( MouseEvent event )
{
    mArcball.mouseDown( event.getPos() );
}

void gpuPSApp::mouseDrag( MouseEvent event )
{
    mArcball.mouseDrag( event.getPos() );
}


void gpuPSApp::draw()
{
	gl::clear( ColorA( 0.0f, 0.0f, 0.0f, 1.0f ) );
	glEnable( mFBO.getTarget() );
	mFBO.bindTexture();
	mShader.bind();
	mShader.uniform("colorMap", 0 );
	mShader.uniform("displacementMap", 0 );
	
	gl::pushModelView();
		gl::translate( Vec3f( 0.0f, 0.0f, getWindowHeight() / 2.0f ) );
		gl::rotate( mArcball.getQuat() );
		gl::draw( mVboMesh );
    gl::popModelView();
	
	mShader.unbind();
	mFBO.unbindTexture();
	//	gl::drawString( toString( SIDE*SIDE ) + " vertices!", Vec2f(32.0f, 32.0f));
	//	gl::drawString( toString((int) getAverageFps()) + " fps", Vec2f(32.0f, 52.0f));
}


CINDER_APP_BASIC( gpuPSApp, RendererGl )