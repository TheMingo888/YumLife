#include "rocketAnimation.h"
#include "minorGems/util/stringUtils.h"
#include "minorGems/util/SimpleVector.h"
#include "minorGems/util/random/JenkinsRandomSource.h"

#include "minorGems/game/drawUtils.h"


extern doublePair lastScreenViewCenter;

extern double visibleViewWidth;
extern double viewHeight;

extern double frameRateFactor;



// relative to screen center
static doublePair rocketPos;
static doublePair rocketStartPos;
static doublePair rocketEndPos;

static JenkinsRandomSource randSource;


typedef struct SpeechInfo {
        int speakerID;
        char *speech;
        
        // relative to screen center
        doublePair pos;

        double fade;
        // wall clock time when speech should start fading
        double fadeETATime;
    } SpeechInfo;


static SimpleVector<SpeechInfo> extraSpeech;


typedef struct StarInfo { 
        // relative to screen center
        doublePair pos;
        double fade;
        
        double twinkleAmount;
        double twinkleProgress;
        double twinkleRate;
    } StarInfo;


SimpleVector<StarInfo> stars;

// in screen units per frame
double starSpeed = 0.5;




static LivingLifePage *page;

static LiveObject *ridingPlayer;

static ObjectRecord *rocket;

static int frameCount;


static double startTime;
static double animationLengthSeconds;

// between 0 and 1
static double progress = 0;

static char running = false;



double getTwinkleRate() {
    return randSource.getRandomBoundedDouble( 0.01, 0.02 );
    }


double getTwinkleAmount( double inFade ) {
    return randSource.getRandomBoundedDouble( inFade * .25, 
                                              inFade * .6 );
    }


void initRocketAnimation( LivingLifePage *inPage, 
                          LiveObject *inRidingPlayer, ObjectRecord *inRocket,
                          double inAnimationLengthSeconds ) {

    randSource.reseed( game_timeSec() );
    
    page = inPage;
    ridingPlayer = inRidingPlayer;
    rocket = inRocket;
    
    frameCount = 0;

    rocketStartPos.x = 0;
    rocketStartPos.y = - viewHeight;
    
    rocketEndPos.x = 0;
    rocketEndPos.y = viewHeight;
    
    rocketPos = rocketStartPos;

    startTime = game_getCurrentTime();
    animationLengthSeconds = inAnimationLengthSeconds;


    for( int i=0; i<1000; i++ ) {
        
        // whole pixel positions
        doublePair posOnScreen = {
            floor( randSource.getRandomBoundedDouble( -visibleViewWidth / 2,
                                                      visibleViewWidth / 2 ) ),
            floor( randSource.getRandomBoundedDouble( -viewHeight / 2,
                                                      // off top of screen
                                                      // so they can move down
                                                      5 * viewHeight ) ) };
        double fade = randSource.getRandomBoundedDouble( 0.25, 1 );
        
        double twinkleAmount = getTwinkleAmount( fade );
        
        double twinkleRate = getTwinkleRate();
        
        double twinkleProgress = randSource.getRandomBoundedDouble( 0, 1 );
        

        StarInfo s = { posOnScreen, fade, twinkleAmount, twinkleProgress,
                       twinkleRate };
        
        stars.push_back( s );
        }
    
    
    running = true;
    
    progress = 0;
    }



void freeRocketAnimation() {
    for( int i=0; i<extraSpeech.size(); i++ ) {
        SpeechInfo *s = extraSpeech.getElement( i );
        
        delete [] s->speech;
        }
    extraSpeech.deleteAll();

    stars.deleteAll();
    }



void stepRocketAnimation() {
    frameCount ++;
    
    double curTime = game_getCurrentTime();
    
    for( int i=0; i<extraSpeech.size(); i++ ) {
        SpeechInfo *s = extraSpeech.getElement( i );
        if( curTime > s->fadeETATime ) {            
            s->fade -= 0.05 * frameRateFactor;
            
            if( s->fade <= 0 ) {
                delete [] s->speech;
                extraSpeech.deleteElement( i );
                i--;
                }
            }
        }
    
    for( int i=0; i<stars.size(); i++ ) {
        StarInfo *s = stars.getElement( i );
        
        double lastProgress = s->twinkleProgress;

        s->twinkleProgress += s->twinkleRate  * 
            frameRateFactor;

        if( floor( s->twinkleProgress ) > floor( lastProgress ) ) {
            // every full whole number of progress, re-roll rate
            s->twinkleRate = getTwinkleRate();
            
            // also re-roll amount
            // note that this will create a discontinuity in the twinkle
            s->twinkleAmount = getTwinkleAmount( s->fade );
            }
        }
    

    double timePassed = game_getCurrentTime() - startTime;
    
    progress = timePassed / animationLengthSeconds;

    rocketPos = add( mult( rocketEndPos, progress ),
                     mult( rocketStartPos, 1.0 - progress ) );
    }




static void setDrawColor( Color *c ) {
    setDrawColor( c->r, c->g, c->b, c->a );
    }



void drawRocketAnimation() {
    Color skyColor( 135 / 255.0, 
                    206 / 255.0, 
                    235 / 255.0 );
    Color spaceColor( 0, 0, 0 );

    Color starColor( 1, 1, 1, 0 );
    
    

    double skyFadeInPoint = 0.025;
    
    double spaceColorProgressPoint = 0.5;
    
    double starsAppearProgressPoint = 0.125;
    
    double starsFullProgressPoint = 0.5;

    Color bgDrawColor;

    if( progress >= spaceColorProgressPoint ) {
        bgDrawColor.setValues( &spaceColor );
        }
    else {
        // fade between sky and space
        double spaceFade = progress / spaceColorProgressPoint;
        
        Color *c = Color::linearSum( &spaceColor, &skyColor, spaceFade );
        
        bgDrawColor.setValues( c );
        delete c;
        }
    
    if( progress >= starsFullProgressPoint ) {
        starColor.a = 1.0f;
        }
    else if( progress >= starsAppearProgressPoint ) {
        starColor.a = 
            ( progress - starsAppearProgressPoint ) 
            /
            ( starsFullProgressPoint - starsAppearProgressPoint );
        }
    
    bgDrawColor.a = 1.0f;
    
    if( progress < skyFadeInPoint ) {
        bgDrawColor.a = progress / skyFadeInPoint;
        }

    setDrawColor( &bgDrawColor );
    
    
    // draw it 2x as big as needed, to be sure there are no
    // edges showing
    drawRect( lastScreenViewCenter,
              visibleViewWidth, viewHeight );

    
    setDrawColor( &starColor );
    
    doublePair starOffset = 
        { 0, 
          -starSpeed * frameCount * frameRateFactor };

    for( int i=0; i<stars.size(); i++ ) {
        StarInfo *s = stars.getElement( i );
        
        double fade = s->fade;
        
        // twinkle reduces brightness by between 0 and twinkleAmount
        fade -=  s->twinkleAmount * 
            0.5 *( sin( s->twinkleProgress * 2 * M_PI ) + 1 );

        setDrawFade( starColor.a * fade );
        
        doublePair pos = add( s->pos, starOffset );

        drawRect( add( pos, lastScreenViewCenter ), 1.5, 1.5 );
        }
    
    }



void addSpeech( int inSpeakerID, const char *inSpeech ) {
    if( inSpeakerID == ridingPlayer->id ) {
        // we handle drawing riding player's speech separately
        return;
        }
    
    // fixme

    double horExtent = 
        0.666 * visibleViewWidth / 2;

    double vertStart = - 0.9 * viewHeight / 2;
    double vertEnd = - 0.4 * viewHeight / 2;
        
    doublePair posOnScreen = { 
        randSource.getRandomBoundedDouble( -horExtent, horExtent ),
        randSource.getRandomBoundedDouble( vertStart, vertEnd ) };
    

    SpeechInfo s = {
        inSpeakerID,
        stringDuplicate( inSpeech ),
        posOnScreen,
        0,
        game_getCurrentTime() + 3 + strlen( inSpeech ) / 5 };
    
    extraSpeech.push_back( s );
    }



char isRocketAnimationRunning() {
    if( ! running ) {
        return false;
        }
    
    if( progress >= 1 ) {
        running = false;
        }
    
    return running;
    }



    
    
