#include "global.h"
#include "event_object_movement.h"
#include "field_camera.h"
#include "field_message_box.h"
#include "field_player_avatar.h"
#include "gpu_regs.h"
#include "main.h"
#include "overworld.h"
#include "pc_platform.h"
#include "pc_shared.h"
#include "pc_touch.h"
#include "script.h"

#include <string.h>

#define TOUCH_SEARCH_RADIUS 32
#define TOUCH_SEARCH_WIDTH (TOUCH_SEARCH_RADIUS * 2 + 1)
#define TOUCH_SEARCH_NODE_COUNT (TOUCH_SEARCH_WIDTH * TOUCH_SEARCH_WIDTH)
#define TOUCH_PATH_MAX_LENGTH 128
#define TOUCH_STEP_TIMEOUT 30
#define TOUCH_NODE_NONE 0xFFFF

enum TouchNodeState
{
    TOUCH_NODE_UNSEEN,
    TOUCH_NODE_OPEN,
    TOUCH_NODE_CLOSED,
};

static u16 sTouchNodeCost[TOUCH_SEARCH_NODE_COUNT];
static u16 sTouchNodeParent[TOUCH_SEARCH_NODE_COUNT];
static u16 sTouchHeap[TOUCH_SEARCH_NODE_COUNT];
static u16 sTouchHeapPosition[TOUCH_SEARCH_NODE_COUNT];
static u8 sTouchNodeState[TOUCH_SEARCH_NODE_COUNT];
static u8 sTouchNodeDirection[TOUCH_SEARCH_NODE_COUNT];
static u16 sTouchHeapSize;

static u8 sTouchPath[TOUCH_PATH_MAX_LENGTH];
static u16 sTouchPathLength;
static u16 sTouchPathPosition;
static s16 sTouchExpectedX;
static s16 sTouchExpectedY;
static u8 sTouchHeldDirection;
static u8 sTouchObjectEventId;
static u8 sTouchObjectLocalId;
static s16 sTouchObjectX;
static s16 sTouchObjectY;
static u16 sTouchStepFrames;
static s8 sTouchMapGroup;
static s8 sTouchMapNum;
static bool8 sTouchNavigationActive;
static bool8 sTouchInteractionPending;
static bool8 sTouchTapPending;
static struct PcTouchEvent sTouchTap;

static const s8 sTouchDirectionX[] = {0, 0, 0, -1, 1};
static const s8 sTouchDirectionY[] = {0, 1, -1, 0, 0};
static const u8 sTouchDirections[] = {DIR_NORTH, DIR_WEST, DIR_EAST, DIR_SOUTH};

static u16 AbsoluteDifference(s16 a, s16 b)
{
    return a >= b ? (u16)(a - b) : (u16)(b - a);
}

static u16 GetNodeIndex(s16 x, s16 y, s16 startX, s16 startY)
{
    s16 localX = x - (startX - TOUCH_SEARCH_RADIUS);
    s16 localY = y - (startY - TOUCH_SEARCH_RADIUS);

    if (localX < 0 || localX >= TOUCH_SEARCH_WIDTH
     || localY < 0 || localY >= TOUCH_SEARCH_WIDTH)
        return TOUCH_NODE_NONE;
    return localY * TOUCH_SEARCH_WIDTH + localX;
}

static void GetNodeCoords(u16 node, s16 startX, s16 startY, s16 *x, s16 *y)
{
    *x = startX - TOUCH_SEARCH_RADIUS + node % TOUCH_SEARCH_WIDTH;
    *y = startY - TOUCH_SEARCH_RADIUS + node / TOUCH_SEARCH_WIDTH;
}

static u16 GetNodeScore(u16 node, s16 startX, s16 startY, s16 targetX, s16 targetY)
{
    s16 x;
    s16 y;

    GetNodeCoords(node, startX, startY, &x, &y);
    return sTouchNodeCost[node]
         + AbsoluteDifference(x, targetX)
         + AbsoluteDifference(y, targetY);
}

static bool32 IsNodeBefore(u16 a,
                           u16 b,
                           s16 startX,
                           s16 startY,
                           s16 targetX,
                           s16 targetY)
{
    u16 aScore = GetNodeScore(a, startX, startY, targetX, targetY);
    u16 bScore = GetNodeScore(b, startX, startY, targetX, targetY);

    if (aScore != bScore)
        return aScore < bScore;
    return sTouchNodeCost[a] > sTouchNodeCost[b];
}

static void SwapHeapNodes(u16 a, u16 b)
{
    u16 node = sTouchHeap[a];

    sTouchHeap[a] = sTouchHeap[b];
    sTouchHeap[b] = node;
    sTouchHeapPosition[sTouchHeap[a]] = a;
    sTouchHeapPosition[sTouchHeap[b]] = b;
}

static void PushOrDecreaseNode(u16 node,
                               s16 startX,
                               s16 startY,
                               s16 targetX,
                               s16 targetY)
{
    u16 position = sTouchHeapPosition[node];

    if (position == TOUCH_NODE_NONE)
    {
        position = sTouchHeapSize++;
        sTouchHeap[position] = node;
        sTouchHeapPosition[node] = position;
    }
    while (position != 0)
    {
        u16 parent = (position - 1) / 2;

        if (!IsNodeBefore(sTouchHeap[position],
                          sTouchHeap[parent],
                          startX,
                          startY,
                          targetX,
                          targetY))
            break;
        SwapHeapNodes(position, parent);
        position = parent;
    }
}

static u16 PopNode(s16 startX, s16 startY, s16 targetX, s16 targetY)
{
    u16 result = sTouchHeap[0];
    u16 position = 0;

    sTouchHeapPosition[result] = TOUCH_NODE_NONE;
    sTouchHeapSize--;
    if (sTouchHeapSize == 0)
        return result;
    sTouchHeap[0] = sTouchHeap[sTouchHeapSize];
    sTouchHeapPosition[sTouchHeap[0]] = 0;
    for (;;)
    {
        u16 left = position * 2 + 1;
        u16 right = left + 1;
        u16 next = position;

        if (left < sTouchHeapSize
         && IsNodeBefore(sTouchHeap[left],
                         sTouchHeap[next],
                         startX,
                         startY,
                         targetX,
                         targetY))
            next = left;
        if (right < sTouchHeapSize
         && IsNodeBefore(sTouchHeap[right],
                         sTouchHeap[next],
                         startX,
                         startY,
                         targetX,
                         targetY))
            next = right;
        if (next == position)
            break;
        SwapHeapNodes(position, next);
        position = next;
    }
    return result;
}

static bool32 BuildPath(struct ObjectEvent *player,
                        s16 startX,
                        s16 startY,
                        s16 targetX,
                        s16 targetY)
{
    u16 startNode;
    u16 targetNode;
    u16 node;
    u16 i;

    sTouchPathLength = 0;
    startNode = GetNodeIndex(startX, startY, startX, startY);
    targetNode = GetNodeIndex(targetX, targetY, startX, startY);
    if (targetNode == TOUCH_NODE_NONE)
        return FALSE;
    memset(sTouchNodeState, TOUCH_NODE_UNSEEN, sizeof(sTouchNodeState));
    memset(sTouchNodeCost, 0xFF, sizeof(sTouchNodeCost));
    memset(sTouchNodeParent, 0xFF, sizeof(sTouchNodeParent));
    memset(sTouchHeapPosition, 0xFF, sizeof(sTouchHeapPosition));
    sTouchHeapSize = 0;
    sTouchNodeCost[startNode] = 0;
    sTouchNodeState[startNode] = TOUCH_NODE_OPEN;
    PushOrDecreaseNode(startNode, startX, startY, targetX, targetY);

    while (sTouchHeapSize != 0)
    {
        s16 x;
        s16 y;

        node = PopNode(startX, startY, targetX, targetY);
        if (node == targetNode)
            break;
        sTouchNodeState[node] = TOUCH_NODE_CLOSED;
        GetNodeCoords(node, startX, startY, &x, &y);
        for (i = 0; i < ARRAY_COUNT(sTouchDirections); i++)
        {
            u8 direction = sTouchDirections[i];
            s16 nextX = x + sTouchDirectionX[direction];
            s16 nextY = y + sTouchDirectionY[direction];
            u16 next = GetNodeIndex(nextX, nextY, startX, startY);
            u16 cost = sTouchNodeCost[node] + 1;

            if (next == TOUCH_NODE_NONE
             || sTouchNodeState[next] == TOUCH_NODE_CLOSED
             || !IsObjectEventPathTilePassable(player,
                                               x,
                                               y,
                                               nextX,
                                               nextY,
                                               direction))
                continue;
            if (cost >= sTouchNodeCost[next])
                continue;
            sTouchNodeCost[next] = cost;
            sTouchNodeParent[next] = node;
            sTouchNodeDirection[next] = direction;
            sTouchNodeState[next] = TOUCH_NODE_OPEN;
            PushOrDecreaseNode(next, startX, startY, targetX, targetY);
        }
    }
    if (sTouchNodeParent[targetNode] == TOUCH_NODE_NONE)
        return targetNode == startNode;

    for (node = targetNode; node != startNode; node = sTouchNodeParent[node])
    {
        if (sTouchPathLength == TOUCH_PATH_MAX_LENGTH)
            return FALSE;
        sTouchPath[sTouchPathLength++] = sTouchNodeDirection[node];
    }
    for (i = 0; i < sTouchPathLength / 2; i++)
    {
        u8 direction = sTouchPath[i];

        sTouchPath[i] = sTouchPath[sTouchPathLength - i - 1];
        sTouchPath[sTouchPathLength - i - 1] = direction;
    }
    return TRUE;
}

static s16 FloorDivideBy16(s32 value)
{
    if (value >= 0)
        return value / 16;
    return -((-value + 15) / 16);
}

static void CancelNavigation(void)
{
    sTouchNavigationActive = FALSE;
    sTouchInteractionPending = FALSE;
    sTouchPathLength = 0;
    sTouchPathPosition = 0;
    sTouchHeldDirection = DIR_NONE;
    sTouchStepFrames = 0;
}

static bool32 BuildObjectInteractionPath(struct ObjectEvent *player,
                                         s16 playerX,
                                         s16 playerY,
                                         u8 objectEventId)
{
    struct ObjectEvent *object = &gObjectEvents[objectEventId];
    u8 bestPath[TOUCH_PATH_MAX_LENGTH];
    u16 bestLength = TOUCH_NODE_NONE;
    u16 i;

    for (i = 0; i < ARRAY_COUNT(sTouchDirections); i++)
    {
        u8 direction = sTouchDirections[i];
        s16 targetX = object->currentCoords.x - sTouchDirectionX[direction];
        s16 targetY = object->currentCoords.y - sTouchDirectionY[direction];

        if (!BuildPath(player, playerX, playerY, targetX, targetY)
         || sTouchPathLength >= bestLength)
            continue;
        bestLength = sTouchPathLength;
        memcpy(bestPath, sTouchPath, bestLength);
    }
    if (bestLength == TOUCH_NODE_NONE)
        return FALSE;
    memcpy(sTouchPath, bestPath, bestLength);
    sTouchPathLength = bestLength;
    sTouchObjectEventId = objectEventId;
    sTouchObjectLocalId = object->localId;
    sTouchObjectX = object->currentCoords.x;
    sTouchObjectY = object->currentCoords.y;
    sTouchInteractionPending = TRUE;
    return TRUE;
}

static u8 GetTappedObjectEventId(s16 targetX, s16 targetY)
{
    u8 objectEventId = GetObjectEventIdByXY(targetX, targetY);

    if (objectEventId == OBJECT_EVENTS_COUNT
     || objectEventId == gPlayerAvatar.objectEventId
     || gObjectEvents[objectEventId].invisible)
    {
        // Object sprites generally occupy the tile above their map position too.
        objectEventId = GetObjectEventIdByXY(targetX, targetY + 1);
    }
    if (objectEventId == OBJECT_EVENTS_COUNT
     || objectEventId == gPlayerAvatar.objectEventId
     || gObjectEvents[objectEventId].invisible)
        return OBJECT_EVENTS_COUNT;
    return objectEventId;
}

static void StartNavigation(const struct PcTouchEvent *event)
{
    struct ObjectEvent *player;
    s16 playerX;
    s16 playerY;
    s16 mapPixelOffsetX;
    s16 mapPixelOffsetY;
    s16 screenX;
    s16 screenY;
    s16 targetX;
    s16 targetY;
    u8 objectEventId;

    CancelNavigation();
    if (!PcPlatformIsOverworldViewportActive()
     || gMain.callback1 != CB1_Overworld
     || ArePlayerFieldControlsLocked()
     || gPlayerAvatar.objectEventId >= OBJECT_EVENTS_COUNT)
        return;
    player = &gObjectEvents[gPlayerAvatar.objectEventId];
    if (!player->active)
        return;
    PlayerGetDestCoords(&playerX, &playerY);
    GetFieldCameraMapPixelOffset(GetGpuReg(REG_OFFSET_BG1HOFS),
                                 GetGpuReg(REG_OFFSET_BG1VOFS),
                                 &mapPixelOffsetX,
                                 &mapPixelOffsetY);
    screenX = event->x - ((s16)event->frameWidth - DISPLAY_WIDTH) / 2;
    screenY = event->y - ((s16)event->frameHeight - DISPLAY_HEIGHT) / 2;
    targetX = gSaveBlock1Ptr->pos.x
            + FloorDivideBy16(screenX + mapPixelOffsetX);
    targetY = gSaveBlock1Ptr->pos.y
            + FloorDivideBy16(screenY + mapPixelOffsetY);
    objectEventId = GetTappedObjectEventId(targetX, targetY);
    if (objectEventId != OBJECT_EVENTS_COUNT)
    {
        if (!BuildObjectInteractionPath(player,
                                        playerX,
                                        playerY,
                                        objectEventId))
            return;
    }
    else if (!BuildPath(player, playerX, playerY, targetX, targetY)
          || sTouchPathLength == 0)
        return;
    sTouchPathPosition = 0;
    sTouchExpectedX = playerX;
    sTouchExpectedY = playerY;
    sTouchHeldDirection = DIR_NONE;
    sTouchStepFrames = 0;
    sTouchMapGroup = gSaveBlock1Ptr->location.mapGroup;
    sTouchMapNum = gSaveBlock1Ptr->location.mapNum;
    sTouchNavigationActive = TRUE;
}

static u16 GetDirectionKey(u8 direction)
{
    switch (direction)
    {
    case DIR_NORTH:
        return DPAD_UP;
    case DIR_SOUTH:
        return DPAD_DOWN;
    case DIR_WEST:
        return DPAD_LEFT;
    case DIR_EAST:
        return DPAD_RIGHT;
    default:
        return 0;
    }
}

static u16 UpdateNavigation(void)
{
    struct ObjectEvent *player;
    s16 playerX;
    s16 playerY;
    s16 nextX;
    s16 nextY;

    if (!sTouchNavigationActive)
        return 0;
    if (!PcPlatformIsOverworldViewportActive()
     || gMain.callback1 != CB1_Overworld
     || ArePlayerFieldControlsLocked()
     || gSaveBlock1Ptr->location.mapGroup != sTouchMapGroup
     || gSaveBlock1Ptr->location.mapNum != sTouchMapNum
     || gPlayerAvatar.objectEventId >= OBJECT_EVENTS_COUNT)
    {
        CancelNavigation();
        return 0;
    }
    player = &gObjectEvents[gPlayerAvatar.objectEventId];
    if (!player->active)
    {
        CancelNavigation();
        return 0;
    }
    PlayerGetDestCoords(&playerX, &playerY);
    if (playerX != sTouchExpectedX || playerY != sTouchExpectedY)
    {
        if (++sTouchStepFrames > TOUCH_STEP_TIMEOUT)
        {
            CancelNavigation();
            return 0;
        }
        return GetDirectionKey(sTouchHeldDirection);
    }
    sTouchStepFrames = 0;
    if (sTouchPathPosition == sTouchPathLength)
    {
        if (sTouchInteractionPending)
        {
            struct ObjectEvent *object;
            u8 direction;

            if (sTouchObjectEventId >= OBJECT_EVENTS_COUNT)
            {
                CancelNavigation();
                return 0;
            }
            object = &gObjectEvents[sTouchObjectEventId];
            if (!object->active
             || object->localId != sTouchObjectLocalId
             || object->currentCoords.x != sTouchObjectX
             || object->currentCoords.y != sTouchObjectY
             || AbsoluteDifference(playerX, sTouchObjectX)
              + AbsoluteDifference(playerY, sTouchObjectY) != 1)
            {
                CancelNavigation();
                return 0;
            }
            if (gPlayerAvatar.tileTransitionState != T_NOT_MOVING)
                return 0;
            direction = GetDirectionToFace(playerX,
                                           playerY,
                                           sTouchObjectX,
                                           sTouchObjectY);
            if (GetPlayerFacingDirection() != direction)
                return GetDirectionKey(direction);
            CancelNavigation();
            return A_BUTTON;
        }
        CancelNavigation();
        return 0;
    }
    sTouchHeldDirection = sTouchPath[sTouchPathPosition++];
    nextX = playerX + sTouchDirectionX[sTouchHeldDirection];
    nextY = playerY + sTouchDirectionY[sTouchHeldDirection];
    if (GetCollisionAtCoords(player,
                             nextX,
                             nextY,
                             sTouchHeldDirection) != COLLISION_NONE)
    {
        CancelNavigation();
        return 0;
    }
    sTouchExpectedX = nextX;
    sTouchExpectedY = nextY;
    return GetDirectionKey(sTouchHeldDirection);
}

u16 PcTouchGetKeys(u16 physicalKeys)
{
    struct PcTouchEvent event;
    u16 keys = 0;

    sTouchTapPending = FALSE;
    if (physicalKeys != 0)
        CancelNavigation();
    while (PcPlatformPopTouchEvent(&event))
    {
        if (event.phase != PC_TOUCH_PHASE_UP
         || (event.flags & PC_TOUCH_FLAG_VIRTUAL_CONTROL))
            continue;
        if (physicalKeys != 0)
            continue;
        if (GetFieldMessageBoxMode() == FIELD_MESSAGE_BOX_NORMAL
         || (PcPlatformIsOverworldViewportActive()
          && gMain.callback1 == CB1_Overworld
          && ArePlayerFieldControlsLocked()))
        {
            sTouchTap = event;
            sTouchTapPending = TRUE;
            CancelNavigation();
            keys |= A_BUTTON;
        }
        else if (!PcPlatformIsOverworldViewportActive())
        {
            sTouchTap = event;
            sTouchTapPending = TRUE;
            keys |= A_BUTTON;
        }
        else
        {
            StartNavigation(&event);
        }
    }
    if (physicalKeys == 0 && keys == 0)
        keys |= UpdateNavigation();
    return keys;
}

bool32 PcTouchConsumeTap(s32 *x, s32 *y)
{
    if (!sTouchTapPending || x == NULL || y == NULL)
        return FALSE;
    *x = sTouchTap.x - ((s32)sTouchTap.frameWidth - DISPLAY_WIDTH) / 2;
    *y = sTouchTap.y - ((s32)sTouchTap.frameHeight - DISPLAY_HEIGHT) / 2;
    sTouchTapPending = FALSE;
    // A is the fallback for screens without semantic touch handling. Once a
    // screen consumes the tap, only its hit-test result should activate UI.
    gMain.heldKeysRaw &= ~A_BUTTON;
    gMain.newKeysRaw &= ~A_BUTTON;
    gMain.heldKeys &= ~A_BUTTON;
    gMain.newKeys &= ~A_BUTTON;
    gMain.newAndRepeatedKeys &= ~A_BUTTON;
    return TRUE;
}
