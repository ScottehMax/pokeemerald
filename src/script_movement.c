#include "global.h"
#include "script_movement.h"
#include "event_object_movement.h"
#include "task.h"
#include "util.h"
#include "constants/event_objects.h"
#include "constants/event_object_movement.h"

static void ScriptMovement_StartMoveObjects(u8 priority);
static u8 GetMoveObjectsTaskId(void);
static bool8 ScriptMovement_TryAddNewMovement(u8 objEventId, const u8 *movementScript);
static u8 GetMovementScriptIdFromObjectEventId(u8 objEventId);
static void ScriptMovement_AddNewMovement(u8 moveScrId, u8 objEventId, const u8 *movementScript);
static void ScriptMovement_UnfreezeActiveObjects(void);
static void ScriptMovement_MoveObjects(u8 taskId);
static void ScriptMovement_TakeStep(u8 moveScrId, u8 objEventId, const u8 *movementScript);

static EWRAM_DATA const u8 *sMovementScripts[OBJECT_EVENTS_COUNT] = {0};
static EWRAM_DATA u8 sMovementObjectEventIds[OBJECT_EVENTS_COUNT] = {0};
static EWRAM_DATA bool8 sMovementScriptFinished[OBJECT_EVENTS_COUNT] = {0};

bool8 ScriptMovement_StartObjectMovementScript(u8 localId, u8 mapNum, u8 mapGroup, const u8 *movementScript)
{
    u8 objEventId;

    if (TryGetObjectEventIdByLocalIdAndMap(localId, mapNum, mapGroup, &objEventId))
        return TRUE;
    if (!FuncIsActiveTask(ScriptMovement_MoveObjects))
        ScriptMovement_StartMoveObjects(50);
    return ScriptMovement_TryAddNewMovement(objEventId, movementScript);
}

bool8 ScriptMovement_IsObjectMovementFinished(u8 localId, u8 mapNum, u8 mapGroup)
{
    u8 objEventId;
    u8 taskId;
    u8 moveScrId;

    if (TryGetObjectEventIdByLocalIdAndMap(localId, mapNum, mapGroup, &objEventId))
        return TRUE;
    taskId = GetMoveObjectsTaskId();
    if (taskId == TASK_NONE)
        return TRUE;
    moveScrId = GetMovementScriptIdFromObjectEventId(objEventId);
    if (moveScrId == OBJECT_EVENTS_COUNT)
        return TRUE;
    return sMovementScriptFinished[moveScrId];
}

void ScriptMovement_UnfreezeObjectEvents(void)
{
    u8 taskId;

    taskId = GetMoveObjectsTaskId();
    if (taskId != TASK_NONE)
    {
        ScriptMovement_UnfreezeActiveObjects();
        DestroyTask(taskId);
    }
}

static void ScriptMovement_StartMoveObjects(u8 priority)
{
    u8 i;

    CreateTask(ScriptMovement_MoveObjects, priority);
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        sMovementScripts[i] = NULL;
        sMovementObjectEventIds[i] = OBJECT_EVENTS_COUNT;
        sMovementScriptFinished[i] = FALSE;
    }
}

static u8 GetMoveObjectsTaskId(void)
{
    return FindTaskIdByFunc(ScriptMovement_MoveObjects);
}

static bool8 ScriptMovement_TryAddNewMovement(u8 objEventId, const u8 *movementScript)
{
    u8 moveScrId;

    moveScrId = GetMovementScriptIdFromObjectEventId(objEventId);
    if (moveScrId != OBJECT_EVENTS_COUNT)
    {
        if (!sMovementScriptFinished[moveScrId])
            return TRUE;

        ScriptMovement_AddNewMovement(moveScrId, objEventId, movementScript);
        return FALSE;
    }
    moveScrId = GetMovementScriptIdFromObjectEventId(OBJECT_EVENTS_COUNT);
    if (moveScrId == OBJECT_EVENTS_COUNT)
        return TRUE;

    ScriptMovement_AddNewMovement(moveScrId, objEventId, movementScript);
    return FALSE;
}

static u8 GetMovementScriptIdFromObjectEventId(u8 objEventId)
{
    u8 i;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        if (sMovementObjectEventIds[i] == objEventId)
            return i;
    }
    return OBJECT_EVENTS_COUNT;
}

static void ScriptMovement_AddNewMovement(u8 moveScrId, u8 objEventId, const u8 *movementScript)
{
    sMovementScriptFinished[moveScrId] = FALSE;
    sMovementScripts[moveScrId] = movementScript;
    sMovementObjectEventIds[moveScrId] = objEventId;
}

static void ScriptMovement_UnfreezeActiveObjects(void)
{
    u8 i;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        if (sMovementObjectEventIds[i] < OBJECT_EVENTS_COUNT)
            UnfreezeObjectEvent(&gObjectEvents[sMovementObjectEventIds[i]]);
    }
}

static void ScriptMovement_MoveObjects(u8 taskId)
{
    u8 i;

    (void)taskId;
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        u8 objEventId = sMovementObjectEventIds[i];

        if (objEventId < OBJECT_EVENTS_COUNT)
            ScriptMovement_TakeStep(i, objEventId, sMovementScripts[i]);
    }
}

static void ScriptMovement_TakeStep(u8 moveScrId, u8 objEventId, const u8 *movementScript)
{
    u8 nextMoveActionId;

    if (ObjectEventIsHeldMovementActive(&gObjectEvents[objEventId])
     && !ObjectEventClearHeldMovementIfFinished(&gObjectEvents[objEventId]))
        return;

    nextMoveActionId = *movementScript;
    if (nextMoveActionId == MOVEMENT_ACTION_STEP_END)
    {
        sMovementScriptFinished[moveScrId] = TRUE;
        FreezeObjectEvent(&gObjectEvents[objEventId]);
    }
    else
    {
        if (!ObjectEventSetHeldMovement(&gObjectEvents[objEventId], nextMoveActionId))
        {
            movementScript++;
            sMovementScripts[moveScrId] = movementScript;
        }
    }
}
