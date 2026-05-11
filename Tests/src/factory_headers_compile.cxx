#include <type_traits>

#include "../../src/allocation_factory.h"

namespace
{
struct Dummy
{
  int value;
};

static_assert(std::is_same<decltype(createObject<Dummy>()), Dummy *>::value, "createObject type");
static_assert(std::is_same<decltype(createArray<int>(2)), int *>::value, "createArray type");
static_assert(std::is_same<decltype(createHighWorkRamObject<Dummy>()), Dummy *>::value, "createHighWorkRamObject type");
static_assert(std::is_same<decltype(createHighWorkRamArray<int>(2)), int *>::value, "createHighWorkRamArray type");

static_assert(std::is_same<decltype(createRandomGenerator(1u)), SRL::Math::Random<unsigned int> *>::value,
              "createRandomGenerator type");
static_assert(std::is_same<decltype(createDigitalGamepad(0)), SRL::Input::Digital *>::value,
              "createDigitalGamepad type");

static_assert(std::is_same<decltype(createBitmapTga("A.TGA")), SRL::Bitmap::TGA *>::value,
              "createBitmapTga type");
static_assert(std::is_same<decltype(createBitmapInfo(1, 1, nullptr)), SRL::Bitmap::BitmapInfo *>::value,
              "createBitmapInfo type");
static_assert(std::is_same<decltype(createCramPalette(SRL::CRAM::TextureColorMode::Paletted16, 0)), SRL::CRAM::Palette *>::value,
              "createCramPalette type");

static_assert(std::is_same<decltype(createWaveSound("A.WAV")), SRL::Sound::Pcm::WaveSound *>::value,
              "createWaveSound type");

static_assert(std::is_same<decltype(createEmbeddedBulletMlParser("A.BLB", nullptr, 0)), BulletMLParserBLB *>::value,
              "createEmbeddedBulletMlParser type");
static_assert(std::is_same<decltype(createFileBulletMlParser("A.XML")), BulletMLParserBLB *>::value,
              "createFileBulletMlParser type");

static_assert(std::is_same<decltype(createBulletMlRuntimeObject<Dummy>()), Dummy *>::value,
              "createBulletMlRuntimeObject type");
static_assert(std::is_same<decltype(createBulletMlRuntimeArray<int>(2)), int *>::value,
              "createBulletMlRuntimeArray type");
}

int main()
{
  return 0;
}
