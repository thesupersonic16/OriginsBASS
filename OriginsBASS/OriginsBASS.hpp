#include "bass.h"
#include "bass_fx.h"

#define RETRO_REV0U   1;
#define RETRO_REV02   1;
#define TILE_SIZE     (16)
#define SFX_COUNT     (0x100)
#define CHANNEL_COUNT (0x10)

// Basic types
typedef signed char int8;
typedef unsigned char uint8;
typedef signed short int16;
typedef unsigned short uint16;
typedef signed int int32;
typedef unsigned int uint32;
typedef uint32 bool32;
typedef uint32 color;


namespace OriginsBASS {
    // Basic structs
    struct Vector2 {
        int32 x;
        int32 y;

        Vector2() : x(0), y(0){};
        Vector2(int32 X, int32 Y) : x(X), y(Y){};
    };

    struct Matrix {

        int32 values[4][4];

        Matrix() {}
    };

    struct String {
        String() {}
    };

    struct SpriteFrame {
        int16 sprX;
        int16 sprY;
        int16 width;
        int16 height;
        int16 pivotX;
        int16 pivotY;
        uint16 duration;
        uint16 unicodeChar;
        uint8 sheetID;
    };
    
    struct Animator {

        enum RotationSyles { RotateNone, RotateFull, Rotate45Deg, Rotate90Deg, Rotate180Deg, RotateStaticFrames };

        SpriteFrame *frames;
        int32 frameID;
        int16 animationID;
        int16 prevAnimationID;
        int16 speed;
        int16 timer;
        int16 frameDuration;
        int16 frameCount;
        uint8 loopIndex;
        uint8 rotationStyle;
    };

    struct Hitbox {
        int16 left;
        int16 top;
        int16 right;
        int16 bottom;
    };

    struct CollisionSensor {
        Vector2 position;
        bool32 collided;
        uint8 angle;
    };

    struct CollisionMask {
        uint8 floorMasks[TILE_SIZE];
        uint8 lWallMasks[TILE_SIZE];
        uint8 rWallMasks[TILE_SIZE];
        uint8 roofMasks[TILE_SIZE];
    };

    struct TileLayer;
    struct ScanlineInfo;
    struct TileInfo;

    enum PrintModes {
        PRINT_NORMAL,
        PRINT_POPUP,
        PRINT_ERROR,
        PRINT_FATAL,
    };

    enum Scopes {
        SCOPE_NONE,
        SCOPE_GLOBAL,
        SCOPE_STAGE,
    };

	// Function Table
    struct RSDKFunctionTable {
        // Registration
        void (*RegisterGlobalVariables)(void **globals, int32 size, void (*initCB)(void *globals));
        void (*RegisterObject)(void **staticVars, const char *name, uint32 entityClassSize, uint32 staticClassSize, void (*update)(void),
                               void (*lateUpdate)(void), void (*staticUpdate)(void), void (*draw)(void), void (*create)(void *), void (*stageLoad)(void),
                               void (*editorLoad)(void), void (*editorDraw)(void), void (*serialize)(void), void (*staticLoad)(void *staticVars));
        void (*RegisterStaticVariables)(void **varClass, const char *name, uint32 classSize);

        // Entities & Objects
        bool32 (*GetActiveEntities)(uint16 group, void **entity);
        bool32 (*GetAllEntities)(uint16 classID, void **entity);
        void (*BreakForeachLoop)(void);
        void (*SetEditableVar)(uint8 type, const char *name, uint8 classID, int32 offset);
        void *(*GetEntity)(uint16 slot);
        int32 (*GetEntitySlot)(void *entity);
        int32 (*GetEntityCount)(uint16 classID, bool32 isActive);
        int32 (*GetDrawListRefSlot)(uint8 drawGroup, uint16 listPos);
        void *(*GetDrawListRef)(uint8 drawGroup, uint16 listPos);
        void (*ResetEntity)(void *entity, uint16 classID, void *data);
        void (*ResetEntitySlot)(uint16 slot, uint16 classID, void *data);
        void *(*CreateEntity)(uint16 classID, void *data, int32 x, int32 y);
        void (*CopyEntity)(void *destEntity, void *srcEntity, bool32 clearSrcEntity);
        bool32 (*CheckOnScreen)(void *entity, Vector2 *range);
        bool32 (*CheckPosOnScreen)(Vector2 *position, Vector2 *range);
        void (*AddDrawListRef)(uint8 drawGroup, uint16 entitySlot);
        void (*SwapDrawListEntries)(uint8 drawGroup, uint16 slot1, uint16 slot2, uint16 count);
        void (*SetDrawGroupProperties)(uint8 drawGroup, bool32 sorted, void (*hookCB)(void));

        // Scene Management
        void (*SetScene)(const char *categoryName, const char *sceneName);
        void (*SetEngineState)(uint8 state);
        void (*ForceHardReset)(bool32 shouldHardReset);
        bool32 (*CheckValidScene)(void);
        bool32 (*CheckSceneFolder)(const char *folderName);
        void (*LoadScene)(void);
        int32 (*FindObject)(const char *name);

        // Cameras
        void (*ClearCameras)(void);
        void (*AddCamera)(Vector2 *targetPos, int32 offsetX, int32 offsetY, bool32 worldRelative);

        // Window/Video Settings
        int32 (*GetVideoSetting)(int32 id);
        void (*SetVideoSetting)(int32 id, int32 value);
        void (*UpdateWindow)(void);

        // Math
        int32 (*Sin1024)(int32 angle);
        int32 (*Cos1024)(int32 angle);
        int32 (*Tan1024)(int32 angle);
        int32 (*ASin1024)(int32 angle);
        int32 (*ACos1024)(int32 angle);
        int32 (*Sin512)(int32 angle);
        int32 (*Cos512)(int32 angle);
        int32 (*Tan512)(int32 angle);
        int32 (*ASin512)(int32 angle);
        int32 (*ACos512)(int32 angle);
        int32 (*Sin256)(int32 angle);
        int32 (*Cos256)(int32 angle);
        int32 (*Tan256)(int32 angle);
        int32 (*ASin256)(int32 angle);
        int32 (*ACos256)(int32 angle);
        int32 (*Rand)(int32 min, int32 max);
        int32 (*RandSeeded)(int32 min, int32 max, int32 *seed);
        void (*SetRandSeed)(int32 seed);
        uint8 (*ATan2)(int32 x, int32 y);

        // Matrices
        void (*SetIdentityMatrix)(Matrix *matrix);
        void (*MatrixMultiply)(Matrix *dest, Matrix *matrixA, Matrix *matrixB);
        void (*MatrixTranslateXYZ)(Matrix *matrix, int32 x, int32 y, int32 z, bool32 setIdentity);
        void (*MatrixScaleXYZ)(Matrix *matrix, int32 x, int32 y, int32 z);
        void (*MatrixRotateX)(Matrix *matrix, int16 angle);
        void (*MatrixRotateY)(Matrix *matrix, int16 angle);
        void (*MatrixRotateZ)(Matrix *matrix, int16 angle);
        void (*MatrixRotateXYZ)(Matrix *matrix, int16 x, int16 y, int16 z);
        void (*MatrixInverse)(Matrix *dest, Matrix *matrix);
        void (*MatrixCopy)(Matrix *matDest, Matrix *matSrc);

        // Strings
        void (*InitString)(String *string, const char *text, uint32 textLength);
        void (*CopyString)(String *dst, String *src);
        void (*SetString)(String *string, const char *text);
        void (*AppendString)(String *string, String *appendString);
        void (*AppendText)(String *string, const char *appendText);
        void (*LoadStringList)(String *stringList, const char *filePath, uint32 charSize);
        bool32 (*SplitStringList)(String *splitStrings, String *stringList, int32 startStringID, int32 stringCount);
        void (*GetCString)(char *destChars, String *string);
        bool32 (*CompareStrings)(String *string1, String *string2, bool32 exactMatch);

        // Screens & Displays
        void (*GetDisplayInfo)(int32 *displayID, int32 *width, int32 *height, int32 *refreshRate, char *text);
        void (*GetWindowSize)(int32 *width, int32 *height);
        int32 (*SetScreenSize)(uint8 screenID, uint16 width, uint16 height);
        void (*SetClipBounds)(uint8 screenID, int32 x1, int32 y1, int32 x2, int32 y2);
        void (*SetScreenVertices)(uint8 startVert2P_S1, uint8 startVert2P_S2, uint8 startVert3P_S1, uint8 startVert3P_S2, uint8 startVert3P_S3);

        // Spritesheets
        uint16 (*LoadSpriteSheet)(const char *filePath, uint8 scope);

        // Palettes & Colors
        void (*SetTintLookupTable)(uint16 *lookupTable);
        void (*SetPaletteMask)(color maskColor);
        void (*SetPaletteEntry)(uint8 bankID, uint8 index, uint32 color);
        color (*GetPaletteEntry)(uint8 bankID, uint8 index);
        void (*SetActivePalette)(uint8 newActiveBank, int32 startLine, int32 endLine);
        void (*CopyPalette)(uint8 sourceBank, uint8 srcBankStart, uint8 destinationBank, uint8 destBankStart, uint8 count);
        void (*LoadPalette)(uint8 bankID, const char *path, uint16 disabledRows);
        void (*RotatePalette)(uint8 bankID, uint8 startIndex, uint8 endIndex, bool32 right);
        void (*SetLimitedFade)(uint8 destBankID, uint8 srcBankA, uint8 srcBankB, int16 blendAmount, int32 startIndex, int32 endIndex);
        void (*BlendColors)(uint8 destBankID, color *srcColorsA, color *srcColorsB, int32 blendAmount, int32 startIndex, int32 count);

        // Drawing
        void (*DrawRect)(int32 x, int32 y, int32 width, int32 height, uint32 color, int32 alpha, int32 inkEffect, bool32 screenRelative);
        void (*DrawLine)(int32 x1, int32 y1, int32 x2, int32 y2, uint32 color, int32 alpha, int32 inkEffect, bool32 screenRelative);
        void (*DrawCircle)(int32 x, int32 y, int32 radius, uint32 color, int32 alpha, int32 inkEffect, bool32 screenRelative);
        void (*DrawCircleOutline)(int32 x, int32 y, int32 innerRadius, int32 outerRadius, uint32 color, int32 alpha, int32 inkEffect,
                                  bool32 screenRelative);
        void (*DrawFace)(Vector2 *vertices, int32 vertCount, int32 r, int32 g, int32 b, int32 alpha, int32 inkEffect);
        void (*DrawBlendedFace)(Vector2 *vertices, color *vertColors, int32 vertCount, int32 alpha, int32 inkEffect);
        void (*DrawSprite)(Animator *animator, Vector2 *position, bool32 screenRelative);
        void (*DrawDeformedSprite)(uint16 sheetID, int32 inkEffect, bool32 screenRelative);
        void (*DrawText)(Animator *animator, Vector2 *position, String *string, int32 endFrame, int32 textLength, int32 align, int32 spacing, void *unused,
                         Vector2 *charOffsets, bool32 screenRelative);
        void (*DrawTile)(uint16 *tiles, int32 countX, int32 countY, Vector2 *position, Vector2 *offset, bool32 screenRelative);
        void (*CopyTile)(uint16 dest, uint16 src, uint16 count);
        void (*DrawAniTiles)(uint16 sheetID, uint16 tileIndex, uint16 srcX, uint16 srcY, uint16 width, uint16 height);
        void (*DrawDynamicAniTiles)(Animator *animator, uint16 tileIndex);
        void (*FillScreen)(uint32 color, int32 alphaR, int32 alphaG, int32 alphaB);

        // Meshes & 3D Scenes
        uint16 (*LoadMesh)(const char *filename, uint8 scope);
        uint16 (*Create3DScene)(const char *identifier, uint16 faceCount, uint8 scope);
        void (*Prepare3DScene)(uint16 sceneIndex);
        void (*SetDiffuseColor)(uint16 sceneIndex, uint8 x, uint8 y, uint8 z);
        void (*SetDiffuseIntensity)(uint16 sceneIndex, uint8 x, uint8 y, uint8 z);
        void (*SetSpecularIntensity)(uint16 sceneIndex, uint8 x, uint8 y, uint8 z);
        void (*AddModelTo3DScene)(uint16 modelFrames, uint16 sceneIndex, uint8 drawMode, Matrix *matWorld, Matrix *matView, color color);
        void (*SetModelAnimation)(uint16 modelFrames, Animator *animator, int16 speed, uint8 loopIndex, bool32 forceApply, int16 frameID);
        void (*AddMeshFrameTo3DScene)(uint16 modelFrames, uint16 sceneIndex, Animator *animator, uint8 drawMode, Matrix *matWorld, Matrix *matView,
                                      color color);
        void (*Draw3DScene)(uint16 sceneIndex);

        // Sprite Animations & Frames
        uint16 (*LoadSpriteAnimation)(const char *filePath, uint8 scope);
        uint16 (*CreateSpriteAnimation)(const char *filePath, uint32 frameCount, uint32 listCount, uint8 scope);
        void (*SetSpriteAnimation)(uint16 aniFrames, uint16 listID, Animator *animator, bool32 forceApply, int16 frameID);
        void (*EditSpriteAnimation)(uint16 aniFrames, uint16 listID, const char *name, int32 frameOffset, uint16 frameCount, int16 speed, uint8 loopIndex,
                                    uint8 rotationStyle);
        void (*SetSpriteString)(uint16 aniFrames, uint16 listID, String *string);
        uint16 (*FindSpriteAnimation)(uint16 aniFrames, const char *name);
        SpriteFrame *(*GetFrame)(uint16 aniFrames, uint16 listID, int32 frameID);
        Hitbox *(*GetHitbox)(Animator *animator, uint8 hitboxID);
        int16 (*GetFrameID)(Animator *animator);
        int32 (*GetStringWidth)(uint16 aniFrames, uint16 listID, String *string, int32 startIndex, int32 length, int32 spacing);
        void (*ProcessAnimation)(Animator *animator);

        // Tile Layers
        uint16 (*GetTileLayerID)(const char *name);
        TileLayer *(*GetTileLayer)(uint16 layerID);
        void (*GetLayerSize)(uint16 layer, Vector2 *size, bool32 usePixelUnits);
        uint16 (*GetTile)(uint16 layer, int32 x, int32 y);
        void (*SetTile)(uint16 layer, int32 x, int32 y, uint16 tile);
        void (*CopyTileLayer)(uint16 dstLayerID, int32 dstStartX, int32 dstStartY, uint16 srcLayerID, int32 srcStartX, int32 srcStartY, int32 countX,
                               int32 countY);
        void (*ProcessParallax)(TileLayer *tileLayer);
        ScanlineInfo *(*GetScanlines)(void);

        // Object & Tile Collisions
        bool32 (*CheckObjectCollisionTouchBox)(void *thisEntity, Hitbox *thisHitbox, void *otherEntity, Hitbox *otherHitbox);
        bool32 (*CheckObjectCollisionTouchCircle)(void *thisEntity, int32 thisRadius, void *otherEntity, int32 otherRadius);
        uint8 (*CheckObjectCollisionBox)(void *thisEntity, Hitbox *thisHitbox, void *otherEntity, Hitbox *otherHitbox, bool32 setPos);
        bool32 (*CheckObjectCollisionPlatform)(void *thisEntity, Hitbox *thisHitbox, void *otherEntity, Hitbox *otherHitbox, bool32 setPos);
        bool32 (*ObjectTileCollision)(void *entity, uint16 collisionLayers, uint8 collisionMode, uint8 collisionPlane, int32 xOffset, int32 yOffset,
                                      bool32 setPos);
        bool32 (*ObjectTileGrip)(void *entity, uint16 collisionLayers, uint8 collisionMode, uint8 collisionPlane, int32 xOffset, int32 yOffset,
                                 int32 tolerance);
        void (*ProcessObjectMovement)(void *entity, Hitbox *outer, Hitbox *inner);
        void (*SetupCollisionConfig)(int32 minDistance, uint8 lowTolerance, uint8 highTolerance, uint8 floorAngleTolerance, uint8 wallAngleTolerance,
                                     uint8 roofAngleTolerance);
        void (*SetPathGripSensors)(CollisionSensor *sensors); // expects 5 sensors
        void (*FindFloorPosition)(CollisionSensor *sensor);
        void (*FindLWallPosition)(CollisionSensor *sensor);
        void (*FindRoofPosition)(CollisionSensor *sensor);
        void (*FindRWallPosition)(CollisionSensor *sensor);
        void (*FloorCollision)(CollisionSensor *sensor);
        void (*LWallCollision)(CollisionSensor *sensor);
        void (*RoofCollision)(CollisionSensor *sensor);
        void (*RWallCollision)(CollisionSensor *sensor);
        int32 (*GetTileAngle)(uint16 tile, uint8 cPlane, uint8 cMode);
        void (*SetTileAngle)(uint16 tile, uint8 cPlane, uint8 cMode, uint8 angle);
        uint8 (*GetTileFlags)(uint16 tile, uint8 cPlane);
        void (*SetTileFlags)(uint16 tile, uint8 cPlane, uint8 flag);
        void (*CopyCollisionMask)(uint16 dst, uint16 src, uint8 cPlane, uint8 cMode);
        void (*GetCollisionInfo)(CollisionMask **masks, TileInfo **tileInfo);

        // Audio
        uint16 (*GetSfx)(const char *path);
        int32 (*PlaySfx)(uint16 sfx, int32 loopPoint, int32 priority);
        void (*StopSfx)(uint16 sfx);
        int32 (*PlayStream)(const char *filename, uint32 channel, uint32 startPos, uint32 loopPoint, bool32 loadASync);
        void (*SetChannelAttributes)(uint8 channel, float volume, float pan, float speed);
        void (*StopChannel)(uint32 channel);
        void (*PauseChannel)(uint32 channel);
        void (*ResumeChannel)(uint32 channel);
        bool32 (*IsSfxPlaying)(uint16 sfx);
        bool32 (*ChannelActive)(uint32 channel);
        uint32 (*GetChannelPos)(uint32 channel);

        // Videos & "HD Images"
        bool32 (*LoadVideo)(const char *filename, double startDelay, bool32 (*skipCallback)(void));
        bool32 (*LoadImage)(const char *filename, double displayLength, double fadeSpeed, bool32 (*skipCallback)(void));

        // Input
        uint32 (*GetInputDeviceID)(uint8 inputSlot);
        uint32 (*GetFilteredInputDeviceID)(bool32 confirmOnly, bool32 unassignedOnly, uint32 maxInactiveTimer);
        int32 (*GetInputDeviceType)(uint32 deviceID);
        bool32 (*IsInputDeviceAssigned)(uint32 deviceID);
        int32 (*GetInputDeviceUnknown)(uint32 deviceID);
        int32 (*InputDeviceUnknown1)(uint32 deviceID, int32 unknown1, int32 unknown2);
        int32 (*InputDeviceUnknown2)(uint32 deviceID, int32 unknown1, int32 unknown2);
        int32 (*GetInputSlotUnknown)(uint8 inputSlot);
        int32 (*InputSlotUnknown1)(uint8 inputSlot, int32 unknown1, int32 unknown2);
        int32 (*InputSlotUnknown2)(uint8 inputSlot, int32 unknown1, int32 unknown2);
        void (*AssignInputSlotToDevice)(uint8 inputSlot, uint32 deviceID);
        bool32 (*IsInputSlotAssigned)(uint8 inputSlot);
        void (*ResetInputSlotAssignments)(void);

        // User File Management
        bool32 (*LoadUserFile)(const char *fileName, void *buffer, uint32 size); // load user file from exe dir
        bool32 (*SaveUserFile)(const char *fileName, void *buffer, uint32 size); // save user file to exe dir

        // Printing (Rev02)
        void (*PrintLog)(int32 mode, const char *message, ...);
        void (*PrintText)(int32 mode, const char *message);
        void (*PrintString)(int32 mode, String *message);
        void (*PrintUInt32)(int32 mode, const char *message, uint32 i);
        void (*PrintInt32)(int32 mode, const char *message, int32 i);
        void (*PrintFloat)(int32 mode, const char *message, float f);
        void (*PrintVector2)(int32 mode, const char *message, Vector2 vec);
        void (*PrintHitbox)(int32 mode, const char *message, Hitbox hitbox);

        // Editor
        void (*SetActiveVariable)(int32 classID, const char *name);
        void (*AddVarEnumValue)(const char *name);

        // Debugging
        void (*ClearViewableVariables)(void);
        void (*AddViewableVariable)(const char *name, void *value, int32 type, int32 min, int32 max);

        // Origins Extras
        void (*NotifyCallback)(int32 callbackID, int32 param1, int32 param2, int32 param3);
        void (*SetGameFinished)(void);
        void (*StopAllSfx)(void);
    };

    extern RSDKFunctionTable *RSDKTable;

} // namespace OriginsBASS

#include "Audio.hpp"
