#pragma once

#include "xmodel.h"

typedef struct aiScene aiScene;
xModel *convertAssimpScene(const aiScene *scene);
xAnimList *convertAssimpAnimations(const aiScene *scene, xModel *mdl);
