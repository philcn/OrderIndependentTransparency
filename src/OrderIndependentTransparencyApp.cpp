#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Camera.h"
#include "cinder/CameraUi.h"
#include "cinder/ObjLoader.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class OrderIndependentTransparencyApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;

	void initShaderStorage();
	void clearBuffers();

	CameraPersp			mCamera;
	CameraUi			mCameraUi;

	gl::GlslProgRef		mOITProgram;
	gl::BatchRef		mModelBatch;
	gl::BatchRef		mFullscreenBatch;

	GLuint				mOITSubroutineIndexPass1;
	GLuint				mOITSubroutineIndexPass2;

	gl::BufferObjRef	mLinkedListBuffer;
	gl::BufferObjRef	mAtomicCounterBuffer;
	gl::Texture2dRef	mHeadPointerTexture;
	gl::PboRef			mHeadPointerClearPbo;
};

void OrderIndependentTransparencyApp::setup()
{
	gl::enableVerticalSync( false );

	try {
		mOITProgram = gl::GlslProg::create( loadAsset( "shaders/oit.vert" ), loadAsset( "shaders/oit.frag" ) );
		mOITSubroutineIndexPass1 = glGetSubroutineIndex( mOITProgram->getHandle(), GL_FRAGMENT_SHADER, "pass1" );
		mOITSubroutineIndexPass2 = glGetSubroutineIndex( mOITProgram->getHandle(), GL_FRAGMENT_SHADER, "pass2" );
	} catch( gl::GlslProgCompileExc &e ) {
		cout << e.what();
	}

	auto modelMesh = gl::VboMesh::create( ObjLoader( loadAsset( "models/r8s.obj" ) ) );
//	auto modelMesh = gl::VboMesh::create( geom::Teapot() );
	mModelBatch = gl::Batch::create( modelMesh, mOITProgram );
	mFullscreenBatch = gl::Batch::create( geom::Rect(), mOITProgram );

	mCamera.setPerspective( 70.0f, getWindowAspectRatio(), 0.1f, 1000.0f );
	mCamera.lookAt( vec3( 1.0f ) * 1.5f, vec3( 0.0f ) );
	mCameraUi.setCamera( &mCamera );
	mCameraUi.connect( getWindow(), -1 );

	initShaderStorage();
}

void OrderIndependentTransparencyApp::initShaderStorage()
{
	mAtomicCounterBuffer = gl::BufferObj::create( GL_ATOMIC_COUNTER_BUFFER, sizeof( GLuint ), nullptr, GL_DYNAMIC_DRAW );

	unsigned maxNumNodes = 20 * getWindowWidth() * getWindowHeight();
	unsigned linkedListNodeSize = 5 * sizeof( GLfloat ) + sizeof( GLuint );
	mLinkedListBuffer = gl::BufferObj::create( GL_SHADER_STORAGE_BUFFER, maxNumNodes * linkedListNodeSize, nullptr, GL_DYNAMIC_DRAW );
	mOITProgram->uniform( "MaxNodes", maxNumNodes );

	auto textureFormat = gl::Texture2d::Format().internalFormat( GL_R32UI ).mipmap( false );
	mHeadPointerTexture = gl::Texture::create( getWindowWidth(), getWindowHeight(), textureFormat );

	vector<GLuint> clearValues( mHeadPointerTexture->getWidth() * mHeadPointerTexture->getHeight(), 0xffffffff );
	mHeadPointerClearPbo = gl::Pbo::create( GL_PIXEL_UNPACK_BUFFER, clearValues.size() * sizeof( GLuint ), clearValues.data(), GL_STATIC_COPY );
}

void OrderIndependentTransparencyApp::mouseDown( MouseEvent event )
{
}

void OrderIndependentTransparencyApp::update()
{
	getWindow()->setTitle( to_string( int( getAverageFps() ) ) );

	vec4 lightPos = vec4( 3, 3, 3, 1 );
	lightPos = glm::rotateY( lightPos, float( getElapsedSeconds() ) * 0.5f );
	mOITProgram->uniform( "LightPosition", lightPos );
	mOITProgram->uniform( "LightIntensity", vec3( 1.5f ) );
	mOITProgram->uniform( "Kd", vec4( 0.9f, 0.5f, 0.3f, 0.3f ) );
	mOITProgram->uniform( "Ka", vec4( 0.2f, 0.2f, 0.4f, 1.0f ) );
}

void OrderIndependentTransparencyApp::clearBuffers()
{
	GLuint zero = 0;
	mAtomicCounterBuffer->bufferSubData( 0, sizeof( GLuint ), &zero );

	mHeadPointerTexture->update( mHeadPointerClearPbo, GL_RED_INTEGER, GL_UNSIGNED_INT, 0, 0 );
}

void OrderIndependentTransparencyApp::draw()
{
	clearBuffers();

	// pass1
	{
		gl::ScopedDepth scopedDepth( false );
		gl::ScopedGlslProg scopedGlslProg( mOITProgram );
		glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &mOITSubroutineIndexPass1 );
		glBindImageTexture( 0, mHeadPointerTexture->getId(), 0, GL_FALSE, 0, GL_READ_WRITE, mHeadPointerTexture->getInternalFormat() );
		gl::bindBufferBase( GL_ATOMIC_COUNTER_BUFFER, 0, mAtomicCounterBuffer );
		gl::bindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, mLinkedListBuffer );

		gl::ScopedMatrices scopedMatrices;
		gl::setMatrices( mCamera );

		gl::ScopedModelMatrix scopedModelMatrices;
		mat4 carTransform = glm::translate( vec3( 0, -0.25f, 0 ) ) * glm::rotate( toRadians( 90.0f ), vec3( 0, 1, 0 ) ) * glm::scale( vec3( 0.9f ) * 0.01f );
		gl::multModelMatrix( carTransform );

		gl::clear( Color( 0, 0, 0 ) ); 
		mModelBatch->draw();

		glFinish();
	}

	// pass2
	if( 1 ) {
		gl::memoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );

		gl::ScopedBlendAlpha scopedBlend;
		gl::ScopedDepth scopedDepth( false );
		gl::ScopedGlslProg scopedGlslProg( mOITProgram );
		glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &mOITSubroutineIndexPass2 );
		glBindImageTexture( 0, mHeadPointerTexture->getId(), 0, GL_FALSE, 0, GL_READ_WRITE, mHeadPointerTexture->getInternalFormat() );
		gl::bindBufferBase( GL_ATOMIC_COUNTER_BUFFER, 0, mAtomicCounterBuffer );
		gl::bindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, mLinkedListBuffer );

		gl::ScopedMatrices scopedMatrices;
		gl::setMatricesWindow( getWindowSize() );

		gl::ScopedModelMatrix scopedModelMatrices;
		gl::translate( getWindowSize() / 2 );
		gl::scale( getWindowSize() );

		gl::clear( Color( 0, 0, 0 ) ); 
		mFullscreenBatch->draw();
	}
}

CINDER_APP( OrderIndependentTransparencyApp, RendererGl( RendererGl::Options().version( 4, 3 ).msaa( 0 ) ), [] ( App::Settings *settings ) {
	settings->setWindowSize( 1200, 800 );
	settings->disableFrameRate();
} )
