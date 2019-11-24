#include "../SDK/PluginSDK.h"
#include "../SDK/EventArgs.h"
#include "../SDK/EventHandler.h"
#include <limits>

PLUGIN_API const char PLUGIN_PRINT_NAME[32] = "Ornn";
PLUGIN_API const char PLUGIN_PRINT_AUTHOR[32] = "BiggieSmalls";
PLUGIN_API ChampionId PLUGIN_TARGET_CHAMP = ChampionId::Ornn;

namespace Menu
{
	IMenu* MenuInstance = nullptr;

	namespace Combo
	{
		IMenuElement* Enabled = nullptr;
		IMenuElement* UseQ = nullptr;
		IMenuElement* UseW = nullptr;
		IMenuElement* UseE = nullptr;
		IMenuElement* UseR = nullptr;
		IMenuElement* Rmin = nullptr;
	}

	namespace Harass
	{
		IMenuElement* Enabled = nullptr;
		IMenuElement* UseQ = nullptr;
		IMenuElement* UseW = nullptr;
		IMenuElement* UseE = nullptr;
		IMenuElement* TowerHarass = nullptr;
		IMenuElement* ManaQ = nullptr;
	}

	namespace LaneClear
	{
		IMenuElement* Enabled = nullptr;
		IMenuElement* UseQ = nullptr;
		IMenuElement* UseW = nullptr;
		IMenuElement* ManaClear = nullptr;
		IMenuElement* MinMinions = nullptr;
	}

	namespace Killsteal
	{
		IMenuElement* Enabled = nullptr;
		IMenuElement* UseQ = nullptr;
		IMenuElement* UseE = nullptr;
		IMenuElement* UseR = nullptr;
	}

	namespace Misc
	{
		IMenuElement* Qinterrupt = nullptr;
		IMenuElement* Antigap = nullptr;
		IMenuElement* AutoHardCC = nullptr;
		IMenuElement* AutoR = nullptr;
		IMenuElement* Wcc = nullptr;
		IMenuElement* AutoE = nullptr;
		IMenuElement* Brittle = nullptr;
	}
	namespace Drawings
	{
		IMenuElement* Toggle = nullptr;
		IMenuElement* DrawQRange = nullptr;
		IMenuElement* DrawWRange = nullptr;
		IMenuElement* DrawERange = nullptr;
		IMenuElement* DrawRRange = nullptr;
	}

	namespace Colors
	{
		IMenuElement* QColor = nullptr;
		IMenuElement* WColor = nullptr;
		IMenuElement* EColor = nullptr;
		IMenuElement* RColor = nullptr;
	}
}

namespace Spells
{
	std::shared_ptr<ISpell> Q = nullptr;
	std::shared_ptr<ISpell> W = nullptr;
	std::shared_ptr<ISpell> E = nullptr;
	std::shared_ptr<ISpell> R = nullptr;
	std::shared_ptr<ISpell> R2 = nullptr;
}
IGameObject* OrnnQPillar = nullptr;
IGameObject* GoatObject = nullptr;

auto get_best_e_position(IGameObject* OrnnTarget)
{
	auto PlayerPos = g_LocalPlayer->Position();
	const auto ERange = Spells::E->Range();

	if (OrnnTarget && OrnnTarget->IsValidTarget() && OrnnTarget->IsVisibleOnScreen())
	{
		auto Deg2Rad = [](auto Deg) { return Deg * 0.01745329251994329575; };
		auto EnemyPos = OrnnTarget->Position();
		auto Delta = (EnemyPos - PlayerPos).Normalized() * ERange;

		std::vector<Vector> PossiblePoints;

	
       for (auto i = 0u; i < 24; i++)
        {
            PossiblePoints.push_back(PlayerPos + Delta.Rotated(Deg2Rad(15.f * i)));
        }
		std::vector<Vector> possible_wall_points;

		for (auto& p : PossiblePoints)
		{
			for (auto x = 0; x < Spells::E->Range(); x += 10)
			{
				const auto pos = PlayerPos.Extend(p, x);
				if (pos.IsWall())
				{
					possible_wall_points.push_back(pos);
					break;
				}
			}
		}

		Vector BestPos = 0;
		for (auto& p : possible_wall_points)
		{
			IGameObject* point = nullptr;
			const auto Prediction = g_Common->GetPrediction(OrnnTarget, 750, 0.35 , 150, 1400, kCollidesWithWalls, p);
			if (BestPos == 0 &&
				Prediction.Hitchance > HitChance::Medium ||
				Prediction.Hitchance >= HitChance::High &&
				OrnnTarget->Distance(p) < OrnnTarget->Distance(BestPos))
			{
				BestPos = p;
			}
		}
		return BestPos;
	}
}
// Tracking Ornn Ult
void OnCreateObject(IGameObject* Object)
{
	if (!Object->IsValid())
		return;

	auto ObjectName = Object->Name();
	if (ObjectName.find("OrnnRWave") != std::string::npos)
	{
		GoatObject = Object;
	}
	if (ObjectName.find("OrnnQPillar") != std::string::npos)
	{
		OrnnQPillar = Object;
	}
}
//number of  enemies in Range
int CountEnemiesInRange(const Vector& Position, const float Range)
{
	auto Enemies = g_ObjectManager->GetChampions(false);
	int Counter = 0;
	for (auto& Enemy : Enemies)
		if (Enemy->IsVisible() && Enemy->IsValidTarget() && Position.Distance(Enemy->Position()) <= Range)
			Counter++;
	return Counter;
}

// returns prediction of spell
auto get_prediction(std::shared_ptr<ISpell> spell, IGameObject* unit) -> IPredictionOutput {
	return g_Common->GetPrediction(unit, spell->Range(), spell->Delay(), spell->Radius(), spell->Speed(), spell->CollisionFlags(),
		g_LocalPlayer->ServerPosition());
}

// killsteal
void KillstealLogic()
{
	const auto Enemies = g_ObjectManager->GetChampions(false);
	for (auto Enemy : Enemies)
	{
		if (Menu::Killsteal::UseR->GetBool() && Spells::R->IsReady() && Enemy->Distance(g_LocalPlayer->Position()) >= 300 && Enemy->IsValidTarget(Spells::R->Range()) && !Enemy->IsInvulnerable() && !Enemy->HasBuff("KayleR") && !Enemy->HasBuff("UndyingRage") && !g_LocalPlayer->HasBuff("ornnrrecastmanager"))
		{
			auto damage = g_Common->CalculateDamageOnUnit(g_LocalPlayer, Enemy, DamageType::Magical, std::vector<double> {125, 175, 225}[Spells::R->Level()
				- 1] + (0.2 * g_LocalPlayer->FlatMagicDamageMod()));

			if (damage >= Enemy->RealHealth(false, true))
				Spells::R->Cast(Enemy, HitChance::VeryHigh);
		}

		if (Menu::Killsteal::UseQ->GetBool() && Spells::Q->IsReady() && Enemy->IsValidTarget(Spells::Q->Range()))
		{
			auto QDamage = g_Common->CalculateDamageOnUnit(g_LocalPlayer, Enemy, DamageType::Physical, std::vector<double> {20, 34, 70, 95, 120}[Spells::Q->Level()
				- 1] + 1.1 * g_LocalPlayer->TotalAttackDamage());
			if (QDamage >= Enemy->RealHealth(true, false))
				Spells::Q->Cast(Enemy, HitChance::VeryHigh);
		}

		if (Menu::Killsteal::UseE->GetBool() && Spells::E->IsReady() && Enemy->IsValidTarget(Spells::E->Range()))
		{
			auto EDamage = g_Common->CalculateDamageOnUnit(g_LocalPlayer, Enemy, DamageType::Physical, std::vector<double> {80, 125, 170, 215, 260}[Spells::E->Level()
				- 1] + (0.4 * g_LocalPlayer->BonusArmor()) + (0.4 * g_LocalPlayer->BonusSpellBlock())); ///dobule check this 40% bonus MR scaling later
			if (EDamage >= Enemy->RealHealth(false, true))
				Spells::E->Cast(Enemy, HitChance::VeryHigh);
		}
	}
}

// W to block incoming CC
void OnProcessSpell(IGameObject* sender, OnProcessSpellEventArgs* args)
{
	if (Menu::Misc::Wcc->GetBool() && Spells::W->IsReady() && sender->IsEnemy())
	{
		// targeted spells
		if (args->Target == g_LocalPlayer)
		{
			if (sender->HasBuff("PowerFist") ||                                     
				sender->HasBuff("LeonaShieldOfDaybreak") ||
				sender->HasBuff("RenektonPreExecute") ||
				sender->HasBuff("GoldCardPreAttack") ||
				sender->HasBuff("VolibearQ"))
				Spells::W->Cast(sender->Position());

			if (args->SpellSlot == SpellSlot::Q)
			{
				if (sender->ChampionId() == ChampionId::Fiddlesticks ||
					sender->ChampionId() == ChampionId::Blitzcrank ||
					sender->ChampionId() == ChampionId::Alistar)
					Spells::W->Cast(sender->Position());
			}

			if (args->SpellSlot == SpellSlot::W)
			{
				if (sender->ChampionId() == ChampionId::Alistar ||
					sender->ChampionId() == ChampionId::Lissandra ||
					sender->ChampionId() == ChampionId::Lulu ||
					sender->ChampionId() == ChampionId::Renekton ||
					sender->ChampionId() == ChampionId::Riven ||
					sender->ChampionId() == ChampionId::Maokai ||
					sender->ChampionId() == ChampionId::MonkeyKing ||
					sender->ChampionId() == ChampionId::Pantheon)
					Spells::W->Cast(sender->Position());
			}

			if (args->SpellSlot == SpellSlot::E)
			{
				if (sender->ChampionId() == ChampionId::Poppy ||
					sender->ChampionId() == ChampionId::Rammus ||
					sender->ChampionId() == ChampionId::Ahri ||
					sender->ChampionId() == ChampionId::Vayne ||
					sender->ChampionId() == ChampionId::Darius)
					Spells::W->Cast(sender->Position());
			}

			if (args->SpellSlot == SpellSlot::R)
			{
				if (sender->ChampionId() == ChampionId::Lissandra ||
					sender->ChampionId() == ChampionId::Nautilus ||
					sender->ChampionId() == ChampionId::Skarner ||
					sender->ChampionId() == ChampionId::Gnar ||
					sender->ChampionId() == ChampionId::Qiyana ||
					sender->ChampionId() == ChampionId::Vi)
					Spells::W->Cast(sender->Position());
			}
		}

		// non targeted spells
		auto EndPosition = args->EndPosition;

		if (args->SpellSlot == SpellSlot::Q)
		{
			if (sender->ChampionId() == ChampionId::Alistar && g_LocalPlayer->Distance(EndPosition) < 365.f)
				Spells::Q->Cast(sender);
		}

		if (args->SpellSlot == SpellSlot::W)
		{
			if (sender->ChampionId() == ChampionId::Riven && g_LocalPlayer->Distance(EndPosition) < 135.f)
				Spells::Q->Cast(sender);
		}

		if (args->SpellSlot == SpellSlot::E)
		{
			if (sender->ChampionId() == ChampionId::Diana && g_LocalPlayer->Distance(EndPosition) < 300.f)
				Spells::Q->Cast(sender);
		}

		if (args->SpellSlot == SpellSlot::R)
		{
			if ((sender->ChampionId() == ChampionId::Amumu && g_LocalPlayer->Distance(EndPosition) < 550.f) ||
				(sender->ChampionId() == ChampionId::Annie && g_LocalPlayer->Distance(EndPosition) < 250.f) ||
				(sender->ChampionId() == ChampionId::Azir && g_LocalPlayer->Distance(EndPosition) < 250.f) ||
				(sender->ChampionId() == ChampionId::Gragas && g_LocalPlayer->Distance(EndPosition) < 350.f) ||
				(sender->ChampionId() == ChampionId::Hecarim && g_LocalPlayer->Distance(EndPosition) < 240.f) ||
				(sender->ChampionId() == ChampionId::Malphite && g_LocalPlayer->Distance(EndPosition) < 270.f) ||
				(sender->ChampionId() == ChampionId::MonkeyKing && g_LocalPlayer->Distance(EndPosition) < 163.f))
				Spells::Q->Cast(sender);
		}
	}
}

// combo
void ComboLogic()
{
	if (Menu::Combo::Enabled->GetBool())
	{
		if (Menu::Combo::UseQ->GetBool() && Spells::Q->IsReady() && !g_LocalPlayer->HasBuff("ornnrrecastmanager"))
		{
			auto Target = g_Common->GetTarget(Spells::Q->Range(), DamageType::Physical);
			if (Target && Target->IsValidTarget())
			{
				if (Target->Distance(g_LocalPlayer) <= 750.f)
					Spells::Q->Cast(Target, HitChance::VeryHigh);
			}
		}

		if (Menu::Combo::UseW->GetBool() && Spells::W->IsReady() && !g_LocalPlayer->HasBuff("ornnrrecastmanager"))
		{
			auto Target = g_Common->GetTarget(Spells::W->Range(), DamageType::Magical);
			if (Target && Target->IsValidTarget() && !Target->HasBuff("OrnnVulnerableDebuff"))
			{
				if (Target->Distance(g_LocalPlayer) <= 300.f)
					Spells::W->Cast(Target, HitChance::VeryHigh);
			}
		}

		//E logic on Wall

		if (Menu::Combo::UseE->GetBool() && Spells::E->IsReady() && !g_LocalPlayer->HasBuff("ornnrrecastmanager") && !g_LocalPlayer->IsUnderMyEnemyTurret())
		{
			auto Target = g_Common->GetTarget(Spells::E->Range(), DamageType::Physical);

			if (Target && Target->IsValidTarget() && Target->IsAIHero())
				if (Target->Distance(get_best_e_position(Target)) < 280)
					Spells::E->Cast(get_best_e_position(Target));

			if (OrnnQPillar != nullptr && Target->Distance(OrnnQPillar->Position()) <= 280.f)
				Spells::E->Cast(Target, HitChance::VeryHigh);
		}

		// R1 intial ult         ##  for some reason not casting at max range
		if (Menu::Combo::UseR->GetBool() && Spells::R->IsReady())
		{
			const auto Allies = g_ObjectManager->GetChampions(true);
			for (auto Ally : Allies)
			{
				auto Target = g_Common->GetTarget(Spells::R->Range(), DamageType::Magical);
				if (Target && Target->IsValidTarget() && Target->IsAIHero() && Ally->Distance(Target) < 800)
				{
					if (CountEnemiesInRange(Target->Position(), 340.f) >= Menu::Misc::AutoR->GetInt() && !g_LocalPlayer->HasBuff("ornnrrecastmanager") && g_LocalPlayer->HealthPercent() >= 25.f) // health check so we can recast it before we die.
						Spells::R->Cast(Target, HitChance::VeryHigh);
				}

				//// R2 re-cast again to max enemy        ### instead of Rmin->Int , get dynamic best position so we never fail if below # enemies to recast ?

				//if (g_LocalPlayer->HasBuff("ornnrrecastmanager") && GoatObject->IsInRange(150.f) && CountEnemiesInRange(Target->Position(), Spells::R2->Range()) >= Menu::Combo::Rmin->GetInt())
				//	Spells::R2->Cast(Target, HitChance::High);
			}
		}
	}
		
}

// harass
void HarassLogic()
{
	if (Menu::Harass::TowerHarass->GetBool() && g_LocalPlayer->IsUnderMyEnemyTurret())
		return;

	if (g_LocalPlayer->ManaPercent() < Menu::Harass::ManaQ->GetInt())
		return;

	if (Menu::Harass::Enabled->GetBool())
	{
		if (Menu::Harass::UseQ->GetBool() && Spells::Q->IsReady() && !g_LocalPlayer->HasBuff("ornnrrecastmanager"))
		{
			auto Target = g_Common->GetTarget(Spells::Q->Range(), DamageType::Physical);
			if (Target && Target->IsValidTarget())
			{
				if (Target->Distance(g_LocalPlayer) <= 750.f)
					Spells::Q->Cast(Target, HitChance::VeryHigh);
			}
		}

		if (Menu::Harass::UseW->GetBool() && Spells::W->IsReady() && !g_LocalPlayer->HasBuff("ornnrrecastmanager"))
		{
			auto Target = g_Common->GetTarget(Spells::W->Range(), DamageType::Magical);
			if (Target && Target->IsValidTarget())
			{
				if (Target->Distance(g_LocalPlayer) <= 300.f)
					Spells::W->Cast(Target, HitChance::VeryHigh);
			}
		}

		//E logic on Wall

		if (Menu::Harass::UseE->GetBool() && Spells::E->IsReady() && !g_LocalPlayer->HasBuff("ornnrrecastmanager"))
		{
			{
				auto Target = g_Common->GetTarget(Spells::E->Range(), DamageType::Physical);

				if (Target && Target->IsValidTarget() && Target->IsAIHero())
					if (Target->Distance(get_best_e_position(Target)) < 300)
						Spells::E->Cast(get_best_e_position(Target));

				if (OrnnQPillar != nullptr && Target->Distance(OrnnQPillar->Position()) <= 280.f)
					Spells::E->Cast(Target, HitChance::VeryHigh);
			}
		}
	}
}

void MiscLogic()
{
	const auto Enemies = g_ObjectManager->GetChampions(false);
	for (auto Enemy : Enemies)
	{
		if (Spells::Q->IsReady() && !g_LocalPlayer->HasBuff("ornnrrecastmanager"))
		{
			if (Enemy && Enemy->IsValidTarget(Spells::Q->Range()))
			{
				const auto pred = get_prediction(Spells::Q, Enemy);
				// Cast Q on Dashing
				if (Menu::Misc::Antigap->GetBool())
					if (pred.Hitchance == HitChance::Dashing)
						Spells::Q->Cast(pred.CastPosition);
				//Cast Q on hard CC
				if (Menu::Misc::AutoHardCC->GetBool())
					if (pred.Hitchance == HitChance::Immobile)
						Spells::Q->Cast(pred.CastPosition);
			}
		}
		 
		if (Spells::Q->IsReady() && !g_LocalPlayer->HasBuff("ornnrrecastmanager"))
		{
			//Cast Q on Channels to hopefully knock them up
			if (Enemy && Enemy->IsValidTarget(Spells::Q->Range()))
			{
				if (Menu::Misc::Qinterrupt->GetBool())
					if (Enemy->IsCastingInterruptibleSpell())
						Spells::Q->Cast(Enemy->ServerPosition());
			}
		}
	}
	if (Spells::E->IsReady() && !g_LocalPlayer->HasBuff("ornnrrecastmanager") && !g_LocalPlayer->IsUnderMyEnemyTurret())
	{
		// Auto E on X enemies into a wall
		auto Target = g_Common->GetTarget(Spells::E->Range(), DamageType::Physical);

		if (Target && Target->IsValidTarget() && Target->IsAIHero() && CountEnemiesInRange(Target->Position(), Spells::E->Range()) >= Menu::Misc::AutoE->GetInt())
			if (Target->Distance(get_best_e_position(Target)) < 250)
				Spells::E->Cast(get_best_e_position(Target));                
	}
	
}

// Lane Clear Logic          ## add E with wall check
void LaneCLearLogic()
{
	if (g_LocalPlayer->ManaPercent() < Menu::LaneClear::ManaClear->GetInt())
		return;

	auto MinMinions = Menu::LaneClear::MinMinions->GetInt();
	if (!MinMinions)
		return;

	auto Target = g_Orbwalker->GetTarget();
	{
		if (Target && (Target->IsMinion() || Target->IsMonster()) && Spells::Q->Radius() && Spells::Q->IsReady() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeLaneClear))

			if (Menu::LaneClear::UseQ->GetBool())
				Spells::Q->CastOnBestFarmPosition(MinMinions);

		if (Target && (Target->IsMinion() || Target->IsMonster()) && Spells::W->Range() && Spells::W->IsReady() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeLaneClear))

			if (Menu::LaneClear::UseW->GetBool())
				Spells::W->CastOnBestFarmPosition(MinMinions);
	}

	// Jungle Clear Logic  ## add E with wall check
	{
		auto Monster = g_Orbwalker->GetTarget();
		{
			if (Monster && Monster->IsMonster() && Menu::LaneClear::UseQ->GetBool() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeLaneClear) && Spells::Q->IsReady())
				Spells::Q->Cast(Monster, HitChance::High);
		}
		{
			if (Monster && Monster->IsMonster() && Spells::W->IsReady() && Menu::LaneClear::UseW->GetBool() && Spells::W->Range() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeLaneClear))

				Spells::W->Cast(Monster, HitChance::High);
		}
	}
}


void OnGameUpdate()
{
	if (g_LocalPlayer->IsDead())
		return;

	// priority for brittle
	const auto Enemies = g_ObjectManager->GetChampions(false);
	for (auto Enemy : Enemies)
	{
		if (Menu::Misc::Brittle->GetBool() && Enemy && Enemy->IsInRange(300) && Enemy->IsValidTarget() && Enemy->HasBuff("OrnnVulnerableDebuff"))
			g_Orbwalker->SetOrbwalkingTarget(Enemy);

	}
	if (Menu::Killsteal::Enabled->GetBool())
		KillstealLogic();

	if (Menu::Combo::Enabled->GetBool() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeCombo))
		ComboLogic();

	if (Menu::Harass::Enabled->GetBool() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeHarass))
		HarassLogic();

	if (Menu::LaneClear::Enabled->GetBool() && g_Orbwalker->IsModeActive(eOrbwalkingMode::kModeLaneClear))
		LaneCLearLogic();

	MiscLogic();


	// Auto R
	const auto Allies = g_ObjectManager->GetChampions(true);
	for (auto Ally : Allies)
		if (Spells::R->IsReady())

		{
			auto Target = g_Common->GetTarget(Spells::R->Range(), DamageType::Magical);
			if (Target && Target->IsValidTarget() && Target->IsAIHero() && Ally->Distance(Target) < 1200)
			{
				if (CountEnemiesInRange(Target->Position(), 340.f) >= Menu::Misc::AutoR->GetInt() && !g_LocalPlayer->HasBuff("ornnrrecastmanager") && g_LocalPlayer->HealthPercent() >= 25.f) // health check so we can recast it before we die.
					Spells::R->Cast(Target, HitChance::VeryHigh);
			}

			// R2 re-cast again to max enemy        ### instead of Rmin->Int , get dynamic best position so we never fail if below # enemies to recast ?

			if (g_LocalPlayer->HasBuff("ornnrrecastmanager") && GoatObject->IsInRange(250.f) && CountEnemiesInRange(Target->Position(), 340.f) >= Menu::Misc::AutoR->GetInt())
				Spells::R2->Cast(Target, HitChance::High);

			if (g_LocalPlayer->HasBuff("ornnrrecastmanager") && GoatObject->IsInRange(250.f) && CountEnemiesInRange(Target->Position(), 340.f) >= 1)
				Spells::R2->Cast(Target, HitChance::High);

		}
}

// Drawings
void OnHudDraw()
{
	if (!Menu::Drawings::Toggle->GetBool() || g_LocalPlayer->IsDead())
		return;

	const auto PlayerPosition = g_LocalPlayer->Position();
	const auto CirclesWidth = 1.5f;

	if (Menu::Drawings::DrawQRange->GetBool() && !Spells::Q->CooldownTime())
		g_Drawing->AddCircle(PlayerPosition, Spells::Q->Range(), Menu::Colors::QColor->GetColor(), CirclesWidth);

	if (Menu::Drawings::DrawWRange->GetBool() && !Spells::W->CooldownTime())
		g_Drawing->AddCircle(PlayerPosition, Spells::W->Range(), Menu::Colors::WColor->GetColor(), CirclesWidth);

	if (Menu::Drawings::DrawERange->GetBool() && !Spells::E->CooldownTime())
		g_Drawing->AddCircle(PlayerPosition, Spells::E->Range(), Menu::Colors::EColor->GetColor(), CirclesWidth);

	if (Menu::Drawings::DrawRRange->GetBool() && !Spells::R->CooldownTime())
		g_Drawing->AddCircle(PlayerPosition, Spells::R->Range(), Menu::Colors::RColor->GetColor(), CirclesWidth);
}

PLUGIN_API bool OnLoadSDK(IPluginsSDK* plugin_sdk)
{
	DECLARE_GLOBALS(plugin_sdk);

	if (g_LocalPlayer->ChampionId() != ChampionId::Ornn)
		return false;

	using namespace Menu;
	using namespace Spells;

	MenuInstance = g_Menu->CreateMenu("Ornn", "Ornn by BiggieSmalls");

	const auto ComboSubMenu = MenuInstance->AddSubMenu("Combo", "combo_menu");
	Menu::Combo::Enabled = ComboSubMenu->AddCheckBox("Enable Combo", "enable_combo", true);
	Menu::Combo::UseQ = ComboSubMenu->AddCheckBox("Use Q", "combo_use_q", true);
	Menu::Combo::UseW = ComboSubMenu->AddCheckBox("Use W", "combo_use_w", true);
	Menu::Combo::UseE = ComboSubMenu->AddCheckBox("Use E", "combo_use_e", true);
	Menu::Combo::UseR = ComboSubMenu->AddCheckBox("Use R", "combo_use_R", true);
	Menu::Combo::Rmin = ComboSubMenu->AddSlider("Use R on # enemies", "min_enemies_range_r", 2, 0, 5);

	const auto HarassSubMenu = MenuInstance->AddSubMenu("Harass", "harass_menu");
	Menu::Harass::Enabled = HarassSubMenu->AddCheckBox("Enable Harass", "enable_harass", true);
	Menu::Harass::UseQ = HarassSubMenu->AddCheckBox("Use Q", "harass_use_q", true);
	Menu::Harass::UseW = HarassSubMenu->AddCheckBox("Use W", "harass_use_w", true);
	Menu::Harass::UseE = HarassSubMenu->AddCheckBox("Use E", "harass_use_ea", false);
	Menu::Harass::TowerHarass = HarassSubMenu->AddCheckBox("Don't Harass under tower", "use_tower_q", true);
	Menu::Harass::ManaQ = HarassSubMenu->AddSlider("Min Mana", "min_mana_harass", 50, 0, 100, true);

	const auto LaneClearSubMenu = MenuInstance->AddSubMenu("Lane Clear", "laneclear_menu");
	Menu::LaneClear::Enabled = LaneClearSubMenu->AddCheckBox("Enable Lane Clear", "enable_laneclear", true);
	Menu::LaneClear::UseQ = LaneClearSubMenu->AddCheckBox("Use Q", "laneclear_use_q", false);
	Menu::LaneClear::UseW = LaneClearSubMenu->AddCheckBox("Use W", "laneclear_use_w", true);
	Menu::LaneClear::ManaClear = LaneClearSubMenu->AddSlider("Min Mana", "min_mana_laneclear", 80, 0, 100, true);
	Menu::LaneClear::MinMinions = LaneClearSubMenu->AddSlider("Min minions", "lane_clear_min_minions", 3, 0, 9);

	const auto KSSubMenu = MenuInstance->AddSubMenu("KS", "ks_menu");
	Menu::Killsteal::Enabled = KSSubMenu->AddCheckBox("Enable Killsteal", "enable_ks", true);
	Menu::Killsteal::UseQ = KSSubMenu->AddCheckBox("Use Q", "q_ks", true);
	Menu::Killsteal::UseE = KSSubMenu->AddCheckBox("Use E", "E_ks", false);
	Menu::Killsteal::UseR = KSSubMenu->AddCheckBox("Use R", "r_ks", true);

	const auto MiscSubMenu = MenuInstance->AddSubMenu("Misc", "misc_menu");
	Menu::Misc::Qinterrupt = MiscSubMenu->AddCheckBox("Interrupt channels", "Q_interupt", true);
	Menu::Misc::Antigap = MiscSubMenu->AddCheckBox("Antigapcloser Q", "q_dashing", true);
	Menu::Misc::AutoHardCC = MiscSubMenu->AddCheckBox("Auto Q on hard CC", "q_hard_cc", true);
	Menu::Misc::Wcc = MiscSubMenu->AddCheckBox("W on displacement CC", "W_hard_cc", true);
	Menu::Misc::Brittle = MiscSubMenu->AddCheckBox("Focus Brittle Debuff", "B_rittl_e", true);
	Menu::Misc::AutoR = MiscSubMenu->AddSlider("Auto R enemies", "min_enemies_range", 4, 0, 5);
	Menu::Misc::AutoE = MiscSubMenu->AddSlider("Auto E if will knockup", "min_enemies_knockup", 3, 0, 5);

	const auto DrawingsSubMenu = MenuInstance->AddSubMenu("Drawings", "drawings_menu");
	Drawings::Toggle = DrawingsSubMenu->AddCheckBox("Enable Drawings", "drawings_toggle", true);
	Drawings::DrawQRange = DrawingsSubMenu->AddCheckBox("Draw Q Range", "draw_q", true);
	Drawings::DrawWRange = DrawingsSubMenu->AddCheckBox("Draw W Range", "draw_w", true);
	Drawings::DrawERange = DrawingsSubMenu->AddCheckBox("Draw E Range", "draw_e", true);
	Drawings::DrawRRange = DrawingsSubMenu->AddCheckBox("Draw R Range", "draw_r", true);

	const auto ColorsSubMenu = DrawingsSubMenu->AddSubMenu("Colors", "color_menu");
	Colors::QColor = ColorsSubMenu->AddColorPicker("Q Range", "color_q_range", 0, 175, 255, 180);
	Colors::WColor = ColorsSubMenu->AddColorPicker("W Range", "color_w_range", 0, 215, 155, 180);
	Colors::EColor = ColorsSubMenu->AddColorPicker("E Range", "color_e_range", 0, 75, 55, 180);
	Colors::RColor = ColorsSubMenu->AddColorPicker("R Range", "color_r_range", 200, 200, 200, 180);

	Spells::Q = g_Common->AddSpell(SpellSlot::Q, 800.f);
	Spells::W = g_Common->AddSpell(SpellSlot::W, 500.f);
	Spells::E = g_Common->AddSpell(SpellSlot::E, 700.f);
	Spells::R = g_Common->AddSpell(SpellSlot::R, 2900.f);
	Spells::R2 = g_Common->AddSpell(SpellSlot::R, 2900.f);

	// pred hitchance is very good with these weird values
	Spells::Q->SetSkillshot(0.3f, 50.f, 1600.f, kCollidesWithYasuoWall, kSkillshotLine); // "OrnnQ"
	Spells::W->SetSkillshot(0.2f, 100.f, 800.f, kCollidesWithNothing, kSkillshotLine); // "OrnnW"               gussing values here
	Spells::E->SetSkillshot(0.35f, 150.f, 1600.f, kCollidesWithWalls, kSkillshotLine); //"OrnnE"
	Spells::R->SetSkillshot(0.5f, 340.f, 1250.f, kCollidesWithYasuoWall, kSkillshotLine); // accelerate from 450 to 1250 between 570 ms by eight irregular interval accelerations at 100 acceleration  "OrnnRWave"
	Spells::R->SetSkillshot(0.5f, 340.f, 1650.f, kCollidesWithYasuoWall, kSkillshotLine); // "OrnnRWave2"

	EventHandler<Events::GameUpdate>::AddEventHandler(OnGameUpdate);
	//EventHandler<Events::OnAfterAttackOrbwalker>::AddEventHandler(OnAfterAttack);
	EventHandler<Events::OnHudDraw>::AddEventHandler(OnHudDraw);
	//	EventHandler<Events::OnBuff>::AddEventHandler(OnBuffChange);
	EventHandler<Events::OnCreateObject>::AddEventHandler(OnCreateObject);
	EventHandler<Events::OnProcessSpellCast>::AddEventHandler(OnProcessSpell);

	g_Common->ChatPrint("<font color='#FFC300'>Ornn Loaded!</font>");
	g_Common->Log("Cho'Gath plugin loaded.");

	return true;
}

PLUGIN_API void OnUnloadSDK()
{
	Menu::MenuInstance->Remove();
	EventHandler<Events::GameUpdate>::RemoveEventHandler(OnGameUpdate);
	EventHandler<Events::GameUpdate>::RemoveEventHandler(OnGameUpdate);
	//EventHandler<Events::OnAfterAttackOrbwalker>::RemoveEventHandler(OnAfterAttack);
	EventHandler<Events::OnHudDraw>::RemoveEventHandler(OnHudDraw);
	//	EventHandler<Events::OnBuff>::RemoveEventHandler(OnBuffChange);
	EventHandler<Events::OnCreateObject>::RemoveEventHandler(OnCreateObject);
	EventHandler<Events::OnProcessSpellCast>::RemoveEventHandler(OnProcessSpell);

	g_Common->ChatPrint("<font color='#00BFFF'>Ornn Unloaded.</font>");
}