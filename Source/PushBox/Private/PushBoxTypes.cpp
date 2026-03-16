// Copyright PushBox Game. All Rights Reserved.

#include "PushBoxTypes.h"

DEFINE_LOG_CATEGORY(LogPushBox);

namespace PushBoxTags
{
	UE_DEFINE_GAMEPLAY_TAG(Message_BoxMoved,          "PushBox.Message.BoxMoved");
	UE_DEFINE_GAMEPLAY_TAG(Message_PlayerMoved,       "PushBox.Message.PlayerMoved");
	UE_DEFINE_GAMEPLAY_TAG(Message_PuzzleStateChanged,"PushBox.Message.PuzzleStateChanged");
	UE_DEFINE_GAMEPLAY_TAG(Message_UndoPerformed,     "PushBox.Message.UndoPerformed");
	UE_DEFINE_GAMEPLAY_TAG(Message_ResetPerformed,    "PushBox.Message.ResetPerformed");

	UE_DEFINE_GAMEPLAY_TAG(Tile_Floor,      "PushBox.Tile.Floor");
	UE_DEFINE_GAMEPLAY_TAG(Tile_Wall,       "PushBox.Tile.Wall");
	UE_DEFINE_GAMEPLAY_TAG(Tile_Ice,        "PushBox.Tile.Ice");
	UE_DEFINE_GAMEPLAY_TAG(Tile_OneWayDoor, "PushBox.Tile.OneWayDoor");
	UE_DEFINE_GAMEPLAY_TAG(Tile_TargetMarker, "PushBox.Tile.TargetMarker");

	UE_DEFINE_GAMEPLAY_TAG(UI_WidgetStack_Game, "Frontend.WidgetStack.Game");
	UE_DEFINE_GAMEPLAY_TAG(UI_WidgetStack_Menu, "Frontend.WidgetStack.Menu");
}
