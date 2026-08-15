/*
 * Copyright (C) 2017 Incognito
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "../main.h"

#include "../natives.h"
#include "../core.h"
#include "../utility.h"

cell AMX_NATIVE_CALL Natives::CreateDynamic3DTextLabel(AMX *amx, cell *params)
{
	CHECK_PARAMS(15);
	if (core->getData()->getGlobalMaxItems(STREAMER_TYPE_3D_TEXT_LABEL) == core->getData()->textLabels.size())
	{
		return INVALID_STREAMER_ID;
	}
	int textLabelId = Item::TextLabel::identifier.get();
	Item::SharedTextLabel textLabel(new Item::TextLabel);
	textLabel->amx = amx;
	textLabel->textLabelId = textLabelId;
	textLabel->inverseAreaChecking = false;
	textLabel->originalComparableStreamDistance = -1.0f;
	textLabel->positionOffset = Eigen::Vector3f::Zero();
	textLabel->streamCallbacks = false;
	textLabel->text = Utility::convertNativeStringToString(amx, params[1]);
	textLabel->color = static_cast<int>(params[2]);
	textLabel->position = Eigen::Vector3f(amx_ctof(params[3]), amx_ctof(params[4]), amx_ctof(params[5]));
	textLabel->drawDistance = amx_ctof(params[6]);
	if (static_cast<int>(params[7]) != INVALID_PLAYER_ID || static_cast<int>(params[8]) != INVALID_VEHICLE_ID)
	{
		textLabel->attach = std::make_shared<Item::TextLabel::Attach>();
		textLabel->attach->player = static_cast<int>(params[7]);
		textLabel->attach->vehicle = static_cast<int>(params[8]);
		if (textLabel->position.cwiseAbs().maxCoeff() > 50.0f)
		{
			textLabel->position.setZero();
		}
		core->getStreamer()->attachedTextLabels.insert(textLabel);
	}
	textLabel->testLOS = static_cast<int>(params[9]) != 0;
	Utility::addToContainer(textLabel->worlds, static_cast<int>(params[10]));
	Utility::addToContainer(textLabel->interiors, static_cast<int>(params[11]));
	Utility::addToContainer(textLabel->players, static_cast<int>(params[12]));
	textLabel->comparableStreamDistance = amx_ctof(params[13]) < STREAMER_STATIC_DISTANCE_CUTOFF ? amx_ctof(params[13]) : amx_ctof(params[13]) * amx_ctof(params[13]);
	textLabel->streamDistance = amx_ctof(params[13]);
	Utility::addToContainer(textLabel->areas, static_cast<int>(params[14]));
	textLabel->priority = static_cast<int>(params[15]);
	core->getGrid()->addTextLabel(textLabel);
	core->getData()->textLabels.insert(std::make_pair(textLabelId, textLabel));
	return static_cast<cell>(textLabelId);
}

cell AMX_NATIVE_CALL Natives::DestroyDynamic3DTextLabel(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	std::unordered_map<int, Item::SharedTextLabel>::iterator t = core->getData()->textLabels.find(static_cast<int>(params[1]));
	if (t != core->getData()->textLabels.end())
	{
		Utility::destroyTextLabel(t);
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::IsValidDynamic3DTextLabel(AMX *amx, cell *params)
{
	CHECK_PARAMS(1);
	std::unordered_map<int, Item::SharedTextLabel>::iterator t = core->getData()->textLabels.find(static_cast<int>(params[1]));
	if (t != core->getData()->textLabels.end())
	{
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamic3DTextLabelText(AMX *amx, cell *params)
{
	CHECK_PARAMS(3);
	std::unordered_map<int, Item::SharedTextLabel>::iterator t = core->getData()->textLabels.find(static_cast<int>(params[1]));
	if (t != core->getData()->textLabels.end())
	{
		cell *text = NULL;
		amx_GetAddr(amx, params[2], &text);
		amx_SetString(text, t->second->text.c_str(), 0, 0, static_cast<size_t>(params[3]));
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::UpdateDynamic3DTextLabelText(AMX *amx, cell *params)
{
	CHECK_PARAMS(3);
	std::unordered_map<int, Item::SharedTextLabel>::iterator t = core->getData()->textLabels.find(static_cast<int>(params[1]));
	if (t != core->getData()->textLabels.end())
	{
		int newColor = static_cast<int>(params[2]);
		std::string newText = Utility::convertNativeStringToString(amx, params[3]);

		if (t->second->color == newColor && t->second->text == newText)
		{
			return 1;
		}

		t->second->color = newColor;
		t->second->text = newText;

		for (std::unordered_map<int, Player>::iterator p = core->getData()->players.begin(); p != core->getData()->players.end(); ++p)
		{
			std::unordered_map<int, int>::iterator i = p->second.internalTextLabels.find(t->first);
			if (i != p->second.internalTextLabels.end())
			{
				ompgdk::UpdatePlayer3DTextLabelText(p->first, i->second, t->second->color, Utility::getTextLabelTextForLanguage(t->second, p->second.language).c_str());
			}
		}
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::UpdateDynamic3DTextLabelLangText(AMX *amx, cell *params)
{
	CHECK_PARAMS(3);
	std::unordered_map<int, Item::SharedTextLabel>::iterator t = core->getData()->textLabels.find(static_cast<int>(params[1]));
	if (t != core->getData()->textLabels.end())
	{
		int language = static_cast<int>(params[2]);
		std::string newText = Utility::convertNativeStringToString(amx, params[3]);

		std::unordered_map<int, std::string>::iterator l = t->second->languageTexts.find(language);
		if (l != t->second->languageTexts.end() && l->second == newText)
		{
			return 1;
		}

		t->second->languageTexts[language] = newText;

		Utility::forEachPlayerWithLanguage(&Player::internalTextLabels, t->first, language, [&](Player &player, int internalId)
		{
			ompgdk::UpdatePlayer3DTextLabelText(player.playerId, internalId, t->second->color, newText.c_str());
		});
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::GetDynamic3DTextLabelLangText(AMX *amx, cell *params)
{
	CHECK_PARAMS(4);
	std::unordered_map<int, Item::SharedTextLabel>::iterator t = core->getData()->textLabels.find(static_cast<int>(params[1]));
	if (t != core->getData()->textLabels.end())
	{
		int language = static_cast<int>(params[2]);
		cell *text = NULL;
		amx_GetAddr(amx, params[3], &text);
		amx_SetString(text, Utility::getTextLabelTextForLanguage(t->second, language).c_str(), 0, 0, static_cast<size_t>(params[4]));
		return 1;
	}
	return 0;
}

cell AMX_NATIVE_CALL Natives::RemoveDynamic3DTextLabelLangText(AMX *amx, cell *params)
{
	CHECK_PARAMS(2);
	std::unordered_map<int, Item::SharedTextLabel>::iterator t = core->getData()->textLabels.find(static_cast<int>(params[1]));
	if (t != core->getData()->textLabels.end())
	{
		int language = static_cast<int>(params[2]);
		std::unordered_map<int, std::string>::iterator l = t->second->languageTexts.find(language);
		if (l == t->second->languageTexts.end())
		{
			return 0;
		}
		t->second->languageTexts.erase(l);

		Utility::forEachPlayerWithLanguage(&Player::internalTextLabels, t->first, language, [&](Player &player, int internalId)
		{
			ompgdk::UpdatePlayer3DTextLabelText(player.playerId, internalId, t->second->color, t->second->text.c_str());
		});
		return 1;
	}
	return 0;
}
