#include "recipelibrarymodule.h"

RecipeLibraryModule::
RecipeLibraryModule() :
  initialized(false) {
  for (
    uint8_t slot = 0;
    slot < MAX_RECIPES;
    slot++
  ) {
    recipes[slot].used = false;
    recipes[slot].name = "";
    recipes[slot].stageCount = 0;
  }
}

bool RecipeLibraryModule::begin() {
  initialized =
    preferences.begin(
      STORAGE_NAMESPACE,
      false
    );

  if (!initialized) {
    Serial.println(
      "Falha ao inicializar a biblioteca de receitas."
    );

    return false;
  }

  loadAll();

  Serial.print(
    "Biblioteca de receitas inicializada. Receitas: "
  );

  Serial.println(
    getRecipeCount()
  );

  return true;
}

bool RecipeLibraryModule::saveRecipe(
  uint8_t slot,
  const String& name,
  const ProfileModule::Stage* stages,
  uint8_t stageCount
) {
  if (
    !initialized ||
    !validateRecipe(
      slot,
      name,
      stages,
      stageCount
    )
  ) {
    return false;
  }

  recipes[slot].used = true;
  recipes[slot].name = name;
  recipes[slot].name.trim();
  recipes[slot].stageCount =
    stageCount;

  for (
    uint8_t index = 0;
    index < stageCount;
    index++
  ) {
    recipes[slot].stages[index] =
      stages[index];
  }

  for (
    uint8_t index = stageCount;
    index < ProfileModule::MAX_STAGES;
    index++
  ) {
    recipes[slot].stages[index]
      .targetTemperature =
        20.0f;

    recipes[slot].stages[index]
      .durationSeconds =
        3600UL;
  }

  return saveSlot(slot);
}

bool RecipeLibraryModule::deleteRecipe(
  uint8_t slot
) {
  if (
    !initialized ||
    slot >= MAX_RECIPES
  ) {
    return false;
  }

  recipes[slot].used = false;
  recipes[slot].name = "";
  recipes[slot].stageCount = 0;

  String prefix =
    "r" + String(slot) + "_";

  preferences.remove(
    (prefix + "used").c_str()
  );

  preferences.remove(
    (prefix + "name").c_str()
  );

  preferences.remove(
    (prefix + "count").c_str()
  );

  for (
    uint8_t index = 0;
    index < ProfileModule::MAX_STAGES;
    index++
  ) {
    preferences.remove(
      (
        prefix +
        "t" +
        String(index)
      ).c_str()
    );

    preferences.remove(
      (
        prefix +
        "d" +
        String(index)
      ).c_str()
    );
  }

  return true;
}

bool RecipeLibraryModule::
loadRecipeIntoProfile(
  uint8_t slot,
  ProfileModule& profile
) {
  if (
    !initialized ||
    slot >= MAX_RECIPES ||
    !recipes[slot].used ||
    profile.isActive()
  ) {
    return false;
  }

  return profile.saveProfile(
    recipes[slot].name,
    recipes[slot].stages,
    recipes[slot].stageCount
  );
}

bool RecipeLibraryModule::startRecipe(
  uint8_t slot,
  ProfileModule& profile
) {
  if (
    !loadRecipeIntoProfile(
      slot,
      profile
    )
  ) {
    return false;
  }

  return profile.start();
}

RecipeLibraryModule::Recipe
RecipeLibraryModule::getRecipe(
  uint8_t slot
) const {
  Recipe emptyRecipe;

  emptyRecipe.used = false;
  emptyRecipe.name = "";
  emptyRecipe.stageCount = 0;

  if (slot >= MAX_RECIPES) {
    return emptyRecipe;
  }

  return recipes[slot];
}

uint8_t RecipeLibraryModule::
getRecipeCount() const {
  uint8_t count = 0;

  for (
    uint8_t slot = 0;
    slot < MAX_RECIPES;
    slot++
  ) {
    if (recipes[slot].used) {
      count++;
    }
  }

  return count;
}

bool RecipeLibraryModule::loadAll() {
  for (
    uint8_t slot = 0;
    slot < MAX_RECIPES;
    slot++
  ) {
    String prefix =
      "r" + String(slot) + "_";

    recipes[slot].used =
      preferences.getBool(
        (prefix + "used").c_str(),
        false
      );

    if (!recipes[slot].used) {
      recipes[slot].name = "";
      recipes[slot].stageCount = 0;
      continue;
    }

    recipes[slot].name =
      preferences.getString(
        (prefix + "name").c_str(),
        ""
      );

    recipes[slot].stageCount =
      preferences.getUChar(
        (prefix + "count").c_str(),
        0
      );

    if (
      recipes[slot].stageCount == 0 ||
      recipes[slot].stageCount >
        ProfileModule::MAX_STAGES
    ) {
      recipes[slot].used = false;
      recipes[slot].stageCount = 0;
      continue;
    }

    for (
      uint8_t index = 0;
      index < recipes[slot].stageCount;
      index++
    ) {
      recipes[slot].stages[index]
        .targetTemperature =
          preferences.getFloat(
            (
              prefix +
              "t" +
              String(index)
            ).c_str(),
            20.0f
          );

      recipes[slot].stages[index]
        .durationSeconds =
          preferences.getULong(
            (
              prefix +
              "d" +
              String(index)
            ).c_str(),
            3600UL
          );
    }
  }

  return true;
}

bool RecipeLibraryModule::saveSlot(
  uint8_t slot
) {
  if (
    !initialized ||
    slot >= MAX_RECIPES
  ) {
    return false;
  }

  String prefix =
    "r" + String(slot) + "_";

  preferences.putBool(
    (prefix + "used").c_str(),
    recipes[slot].used
  );

  size_t nameBytes =
    preferences.putString(
      (prefix + "name").c_str(),
      recipes[slot].name
    );

  preferences.putUChar(
    (prefix + "count").c_str(),
    recipes[slot].stageCount
  );

  for (
    uint8_t index = 0;
    index < recipes[slot].stageCount;
    index++
  ) {
    preferences.putFloat(
      (
        prefix +
        "t" +
        String(index)
      ).c_str(),
      recipes[slot].stages[index]
        .targetTemperature
    );

    preferences.putULong(
      (
        prefix +
        "d" +
        String(index)
      ).c_str(),
      recipes[slot].stages[index]
        .durationSeconds
    );
  }

  return nameBytes > 0;
}

bool RecipeLibraryModule::validateRecipe(
  uint8_t slot,
  const String& name,
  const ProfileModule::Stage* stages,
  uint8_t stageCount
) const {
  String normalizedName =
    name;

  normalizedName.trim();

  if (
    slot >= MAX_RECIPES ||
    normalizedName.length() == 0 ||
    normalizedName.length() > 31 ||
    stages == nullptr ||
    stageCount == 0 ||
    stageCount >
      ProfileModule::MAX_STAGES
  ) {
    return false;
  }

  for (
    uint8_t index = 0;
    index < stageCount;
    index++
  ) {
    if (
      stages[index].targetTemperature <
        -10.0f ||
      stages[index].targetTemperature >
        40.0f ||
      stages[index].durationSeconds <
        60UL ||
      stages[index].durationSeconds >
        90UL * 24UL * 60UL * 60UL
    ) {
      return false;
    }
  }

  return true;
}
