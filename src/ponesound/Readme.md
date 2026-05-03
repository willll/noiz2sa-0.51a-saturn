# Ponesound-SRL
C++ wrapper for using the Ponesound library with SRL.

Supports CDDA, PCM, and ADX playback.

Includes a custom packed/compressed format for PCM samples (`.snd`) using LZSS compression.

PCM and ADX streaming may be added in the future.

## Use
Copy the `Sample` and the `modules_extra` folders to your SRL folder.

Also requires the SMPC and Decompression modules:
* https://github.com/bimmerlabs/smpc-SRL  
* https://github.com/bimmerlabs/Decompression-SRL

To include a module in your project, add this line to your project’s Makefile:
```
MODULES_EXTRA = ponesound smpc decompression
```
Include it in your code:
```
#include <ponesound.hpp>
using namespace SRL::Ponesound;
```

## Sound (.snd) Format

The `.snd` format is a packed and LZSS-compressed container for PCM samples.  

Multiple `.pcm` files can be stored in a single `.snd` file, and multiple `.snd` files can be defined in a project.

### Workflow

1. Place PCM samples in the project `_ASSETS/sfx` folder.
2. Define sound banks and PCM parameters in `SOUND.json`.
3. Run `PcmCompress.py`.
4. Generated `.snd` files are copied automatically to `cd/data` and can be loaded by `SRL::Ponesound`.

## SOUND.json

`SOUND.json` defines:
- `.snd` container files
- `.pcm` files included in each container
- PCM parameters for each sample

Example:
```json
{
  "CAT.SND": {
    "MEOW1.PCM": {
      "SampleRate": "15360",
      "BitDepth": "1"
    },
    "MEOW2.PCM": {
      "SampleRate": "15360",
      "BitDepth": "1"
    }
  },
  "ANOTHER.SND": {
    "MEOW1.PCM": {
      "SampleRate": "15360",
      "BitDepth": "1"
    }
  }
}
```
## Parameters

**`SampleRate`**  
* Sample rate of the PCM file. This is defined when the PCM files are generated (typically via ffmpeg).

**`BitDepth`**  
* PCM bit depth flag:
  - `1` = 8-bit PCM
  - `0` = 16-bit PCM

Notes:
* Multiple `.pcm` files can be grouped into a single `.snd`.
* Multiple `.snd` files can be defined in `SOUND.json`.

## Asset Layout

Place `SOUND.json` and all `.pcm` files in:
```
PROJECT_ROOT/_ASSETS/sfx/
```
The compression script uses the following paths:
```
PROJECT_ROOT = Path(__file__).resolve().parent.parent
INPUT  = PROJECT_ROOT / "_ASSETS" / "sfx"
OUTPUT = PROJECT_ROOT / "cd" / "data"

if __name__ == "__main__":
    packSndInFolder(INPUT, OUTPUT)
```
After running `PcmCompress.py`, the generated `.snd` files are copied to:
```
PROJECT_ROOT/cd/data/
```
These files can then be loaded and played using the `SRL::Ponesound` module.
## Credits
Original Ponesound driver by Ponut64
* https://github.com/ponut64/SCSP_poneSound

Thanks to robertoduarte for the starting point & ReyeMe for the advice.






