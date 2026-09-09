#include <assimp/BaseImporter.h>

namespace rw { struct Clump; }

namespace Assimp {

class DFFImporter : public BaseImporter {
public:
	DFFImporter();
	~DFFImporter() override;

	bool CanRead(const std::string &filename, IOSystem *pIOHandler, bool checkSig) const override {
		return true;
	}

protected:
	const aiImporterDesc *GetInfo() const override;

	void InternReadFile(const std::string &pFile, aiScene *pScene, IOSystem *pIOHandler) override;
};

rw::Clump *assimpSceneToRwClump(const aiScene *scene);
int writeAssimpSceneAsDFF(const aiScene *scene, const char *path);
}
