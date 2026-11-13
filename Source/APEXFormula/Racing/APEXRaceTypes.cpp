#include "Racing/APEXRaceTypes.h"

namespace
{
	FAPEXTeam MakeTeam(const TCHAR* Name, FLinearColor P, FLinearColor S, float Pace)
	{
		FAPEXTeam T; T.Name = Name; T.Primary = P; T.Secondary = S; T.Pace = Pace; return T;
	}
	FAPEXDriver MakeDriver(const TCHAR* Name, const TCHAR* Code, int32 Number, int32 Team, float Skill, float Aggr, float Cons)
	{
		FAPEXDriver D; D.Name = Name; D.Code = Code; D.Number = Number; D.TeamIndex = Team; D.Skill = Skill; D.Aggression = Aggr; D.Consistency = Cons; return D;
	}
}

TArray<FAPEXTeam> UAPEXGridLibrary::GetTeams()
{
	// All fictional. Colours are linear and chosen to be distinguishable at a glance in the timing tower.
	return {
		MakeTeam(TEXT("APEX Racing"),        FLinearColor(0.75f, 0.02f, 0.03f), FLinearColor(0.88f, 0.88f, 0.86f), 1.000f),
		MakeTeam(TEXT("Nova Dynamics"),      FLinearColor(0.02f, 0.10f, 0.55f), FLinearColor(0.95f, 0.75f, 0.05f), 0.998f),
		MakeTeam(TEXT("Vertex Motorsport"),  FLinearColor(1.00f, 0.27f, 0.02f), FLinearColor(0.02f, 0.02f, 0.03f), 0.996f),
		MakeTeam(TEXT("Helix Racing"),       FLinearColor(0.00f, 0.55f, 0.55f), FLinearColor(0.88f, 0.88f, 0.86f), 0.994f),
		MakeTeam(TEXT("Orion Performance"),  FLinearColor(0.30f, 0.08f, 0.55f), FLinearColor(0.70f, 0.70f, 0.75f), 0.990f),
		MakeTeam(TEXT("Pulse Racing"),       FLinearColor(0.85f, 0.05f, 0.45f), FLinearColor(0.02f, 0.02f, 0.03f), 0.987f),
		MakeTeam(TEXT("Quantum GP"),         FLinearColor(0.05f, 0.50f, 0.15f), FLinearColor(0.88f, 0.88f, 0.86f), 0.984f),
		MakeTeam(TEXT("AeroFusion"),         FLinearColor(0.55f, 0.80f, 0.05f), FLinearColor(0.02f, 0.05f, 0.25f), 0.980f),
		MakeTeam(TEXT("Titan Engineering"),  FLinearColor(0.35f, 0.37f, 0.40f), FLinearColor(1.00f, 0.40f, 0.02f), 0.975f),
		MakeTeam(TEXT("Zephyr Racing"),      FLinearColor(0.85f, 0.88f, 0.92f), FLinearColor(0.10f, 0.30f, 0.80f), 0.970f),
	};
}

TArray<FAPEXDriver> UAPEXGridLibrary::GetDrivers()
{
	return {
		MakeDriver(TEXT("Alex Mercer"),     TEXT("AMR"), 27, 0, 1.000f, 0.50f, 0.980f),   // the player
		MakeDriver(TEXT("Luca Marchetti"),  TEXT("LMA"),  1, 0, 0.997f, 0.60f, 0.970f),
		MakeDriver(TEXT("Ren Takahashi"),   TEXT("RTA"),  3, 1, 0.999f, 0.50f, 0.985f),
		MakeDriver(TEXT("Elias Brandt"),    TEXT("EBR"),  4, 1, 0.995f, 0.70f, 0.970f),
		MakeDriver(TEXT("Mateo Alvarez"),   TEXT("MAL"),  5, 2, 0.996f, 0.80f, 0.960f),
		MakeDriver(TEXT("Noah Kessler"),    TEXT("NKE"),  7, 2, 0.992f, 0.40f, 0.985f),
		MakeDriver(TEXT("Aiden Okoro"),     TEXT("AOK"),  8, 3, 0.994f, 0.60f, 0.975f),
		MakeDriver(TEXT("Sofia Lindqvist"), TEXT("SLI"), 10, 3, 0.991f, 0.50f, 0.985f),
		MakeDriver(TEXT("Jonas Varga"),     TEXT("JVA"), 11, 4, 0.990f, 0.75f, 0.960f),
		MakeDriver(TEXT("Kai Nakamura"),    TEXT("KNA"), 14, 4, 0.988f, 0.45f, 0.980f),
		MakeDriver(TEXT("Daniel Rossi"),    TEXT("DRO"), 16, 5, 0.987f, 0.85f, 0.950f),
		MakeDriver(TEXT("Theo Laurent"),    TEXT("TLA"), 18, 5, 0.985f, 0.50f, 0.975f),
		MakeDriver(TEXT("Omar Haddad"),     TEXT("OHA"), 20, 6, 0.984f, 0.65f, 0.965f),
		MakeDriver(TEXT("Victor Salgado"),  TEXT("VSA"), 22, 6, 0.982f, 0.55f, 0.970f),
		MakeDriver(TEXT("Isla McBride"),    TEXT("IMC"), 23, 7, 0.980f, 0.60f, 0.970f),
		MakeDriver(TEXT("Pavel Novak"),     TEXT("PNO"), 31, 7, 0.978f, 0.70f, 0.960f),
		MakeDriver(TEXT("Hugo Ferreira"),   TEXT("HFE"), 44, 8, 0.976f, 0.50f, 0.975f),
		MakeDriver(TEXT("Yara Lindgren"),   TEXT("YLI"), 55, 8, 0.974f, 0.80f, 0.950f),
		MakeDriver(TEXT("Dmitri Volkov"),   TEXT("DVO"), 63, 9, 0.972f, 0.65f, 0.960f),
		MakeDriver(TEXT("Cai Wen"),         TEXT("CWE"), 77, 9, 0.970f, 0.40f, 0.980f),
	};
}
