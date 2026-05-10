#include <srl.hpp>
#include <ponesound.hpp>

using namespace SRL::Types;
using namespace SRL::Input;
using namespace SRL::Ponesound;

const int16_t maxSamples = 9;

int main()
{
	SRL::Core::Initialize(SRL::Types::HighColor::Colors::Black);
    
    short catSnd[maxSamples] = {};
    int16_t volume = 15;
    int16_t curSample = 0;
    int32_t currentTrack = 2;
    bool playCDDA = false;
    
    SRL::Debug::Print(1,3,  "Loading...");
    
    // Ponesound
    Sound::Driver::Initialize(ADXMode::ADX2304);    
    Pcm::LoadSound("CAT.SND", catSnd, maxSamples);           // load compressed sound sample library (.snd)
    int16_t bumpPcm16    = Pcm::Load16("BUMP16.PCM", 15360); // explicitly specify bitrate
    int16_t gameOverPcm8 = Pcm::Load8("GMOVR8.PCM");         // or use the default (15360)
    int16_t adx4snd      = Pcm::LoadAdx("NBGM.ADX");         // load ADX music (large file, takes a long time to load)

    SRL::Debug::Print(1,3, "NumberOfPCMs %d", Sound::GetNumberOfPCMs());
    SRL::Debug::Print(1,4, "CD Volume %d  ", volume);
    SRL::Debug::Print(1,5, "Cat sound %d     ", curSample+1);
    SRL::Debug::Print(1,8,  "Controls:");
    SRL::Debug::Print(5,9,  "Start: start/stop CDDA playback");
    SRL::Debug::Print(5,10, "R/L trigger: previous/next track");
    SRL::Debug::Print(5,11, "Up/Down: - master volume control");
    SRL::Debug::Print(5,12, "Left/Right: - change cat sound");
    SRL::Debug::Print(5,13, "A: - PCM playback (volatile)");
    SRL::Debug::Print(5,14, "B: - PCM playback (semi)");
    SRL::Debug::Print(5,15, "C: - PCM playback (protected)");
    SRL::Debug::Print(5,16, "X: - ADX playback");
    SRL::Debug::Print(5,17, "Y: - ADX stop");
   
    Digital port0(0);

	while(1)
	{
        DateTime time = DateTime::Now();
        SRL::Debug::Print(1,1, "%d:%d:%d %d.%d.%d    ", time.Hour(), time.Minute(), time.Second(), time.Day(), time.Month(), time.Year());
    
        if (port0.WasPressed(Digital::Button::A))
        {
            SRL::Debug::Print(1,6, "Play sample %d ", catSnd[curSample]+1);
            // explicitly specify the playmode and volume
            Pcm::Play(catSnd[curSample], PlayMode::Volatile, 7);
        }
        if (port0.WasPressed(Digital::Button::B))
        {
            SRL::Debug::Print(1,6, "Play sample %d ", bumpPcm16+1);
            // explicitly specify the playmode and use default volume (7)
            Pcm::Play(bumpPcm16, PlayMode::Semi);
        }
        if (port0.WasPressed(Digital::Button::C))
        {
            SRL::Debug::Print(1,6, "Play sample %d", gameOverPcm8+1);
            // use default playmode (Protected) and default volume (7)
            Pcm::Play(gameOverPcm8);
        }
        if (port0.WasPressed(Digital::Button::X))
        {
            SRL::Debug::Print(1,6, "Play adx %d        ", adx4snd);
            Pcm::Play(adx4snd, PlayMode::Semi, 7);
        }
        if (port0.WasPressed(Digital::Button::Y))
        {
            SRL::Debug::Print(1,6, "Stop adx %d        ", adx4snd);
            Pcm::Stop(adx4snd);
        }
        
        if (port0.WasPressed(Digital::Button::START))
        {
            if (!playCDDA)
            {
                SRL::Debug::Print(1,6, "Play Track %d   ", currentTrack);
                CD::PlaySingle(currentTrack, true);
            }
            else
            {
                SRL::Debug::Print(1,6, "Stop CDDA       ");
                CD::Stop();
            }
            playCDDA = !playCDDA;
        }
        if (port0.WasPressed(Digital::Button::R) && playCDDA)
        {
            currentTrack++;
            if (currentTrack > 5)
            {
                currentTrack = 2;
            }
            SRL::Debug::Print(1,6, "Play Track %d   ", currentTrack);
            CD::PlaySingle(currentTrack, true);
        }
        if (port0.WasPressed(Digital::Button::L) && playCDDA)
        {
            currentTrack--;
            if (currentTrack < 2)
            {
                currentTrack = 5;
            }
            SRL::Debug::Print(1,6, "Play Track %d       ", currentTrack);
            CD::PlaySingle(currentTrack, true);
        }
        
        if (port0.WasPressed(Digital::Button::Up))
        {
            volume++;
            if (volume > 15)
            {
                volume = 15;
            }
            SRL::Debug::Print(1,4, "CD Volume %d  ", volume);
            Sound::Driver::SetMasterVolume(volume);
        }
        if (port0.WasPressed(Digital::Button::Down))
        {
            volume--;
            if (volume < 0)
            {
                volume = 0;
            }
            SRL::Debug::Print(1,4, "CD Volume %d  ", volume);
            Sound::Driver::SetMasterVolume(volume);
        }        
        if (port0.WasPressed(Digital::Button::Right))
        {
            curSample++;
            if (curSample > maxSamples-1)
            {
                curSample = maxSamples-1;
            }
            SRL::Debug::Print(1,5, "Cat sound %d     ", curSample+1);
        }
        if (port0.WasPressed(Digital::Button::Left))
        {
            curSample--;
            if (curSample < 0)
            {
                curSample = 0;
            }
            SRL::Debug::Print(1,5, "Cat sound %d     ", curSample+1);
        }
        
        SRL::Core::Synchronize();
	}

	return 0;
}
