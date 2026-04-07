class CCamera
{
public:
	glm::vec3 m_position;
	glm::vec3 m_target;
	glm::vec3 m_up;
	glm::vec3 m_localup;
	glm::vec3 m_at;
	glm::mat4 m_viewMat;
	glm::mat4 m_projMat;

	float m_near, m_far;
	float m_fov, m_aspectRatio;

	void Process(void);
	void DrawTarget(void);

	void setTarget(glm::vec3 target);
	float getHeading(void);

	void turn(float yaw, float pitch);
	void orbit(float yaw, float pitch);
	void dolly(float dist);
	void zoom(float dist);
	void pan(float x, float y);
	void setDistanceFromTarget(float dist);

	void update(void);
	float distanceTo(glm::vec3 v);
	float distanceToTarget(void);
//	float minDistToSphere(float r);
	CCamera(void);

//	bool IsSphereVisible(rw::Sphere *sph, rw::Matrix *xform);
};
