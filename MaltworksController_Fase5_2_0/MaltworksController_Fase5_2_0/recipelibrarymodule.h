#ifndef RECIPELIBRARYMODULE_H
#define RECIPELIBRARYMODULE_H

#include <Arduino.h>
#include <Preferences.h>

#include "profilemodule.h"

class RecipeLibraryModule {
public:
  static constexpr uint8_t MAX_RECIPES = 6;

  struct Recipe {
    bool used;
    String name;
    uint8_t stageCount;
    ProfileModule::Stage stages[
      ProfileModule::MAX_STAGES
    ];
  };

  RecipeLibraryModule();

  bool begin();

  bool saveRecipe(
    uint8_t slot,
    const String& name,
    const ProfileModule::Stage* stages,
    uint8_t stageCount
  );

  bool deleteRecipe(
    uint8_t slot
  );

  bool loadRecipeIntoProfile(
    uint8_t slot,
    ProfileModule& profile
  );

  bool startRecipe(
    uint8_t slot,
    ProfileModule& profile
  );

  Recipe getRecipe(
    uint8_t slot
  ) const;

  uint8_t getRecipeCount() const;

private:
  Preferences preferences;
  Recipe recipes[MAX_RECIPES];
  bool initialized;

  static constexpr const char*
    STORAGE_NAMESPACE =
      "mwrecipes";

  bool loadAll();
  bool saveSlot(
    uint8_t slot
  );

  bool validateRecipe(
    uint8_t slot,
    const String& name,
    const ProfileModule::Stage* stages,
    uint8_t stageCount
  ) const;
};

#endif
