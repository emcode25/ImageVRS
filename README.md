# ImageVRS

## Importing
1. Download and unzip to your project's `Plugins/` folder. 
2. In the Unreal Editor, open the Plugins tab and ensure that the plugin is enabled.

## Usage
ImageVRS requires a specified image texture. Currently, it only supports 1x1, 2x2, and 4x4 shading rates.

For Blueprints, the function `SetVRSTexture` should be used to set a texture.

For C++, the same function should be used which is defined in the `ImageVRSManager.h` file.

To activate ImageVRS, you must use the console command `r.VRS.CustomImage 1` after setting a texture. For C++ users, this can be done using `GEngine->Exec(GetWorld(), *cmd)` where `cmd` is an `FString` containing the command specified.

To ensure that it is working properly, use the command `r.VRS.Preview 1` in the editor to show the active shading regions.

## Texture Format
### Example
![Example of a VRS texture.](Content/Textures/exampleTexture.png?raw=true "Title")

### Color Map
The image colors should be the following for each shading rate (in RGBA).
- `1x1:  #000000FF`
- `2x2:  #555500FF`
- `4x4:  #AAAA00FF`

### Texture Size
The size depends on the hardware's specifications for VRS tile size. Each tile represents a region that VRS will shade at a particular rate. The tile size will be a grouping of 8, 16, or 32 pixels wide.

The texture size should be the resolution of the screen divided by the smallest tile size possible (16 is generally a good guess). For example, a `1920x1080` resolution screen with a minimum tile size of 16 should have a VRS texture with a size of `120x68`. 1080 is not divisible by 16, so the result is rounded up.

### Import Settings
In Unreal's texture editor, the compression should be set to **UserInterface2D** or similar for correct results. The image should also use "nearest" sampling to avoid any color ambiguity.