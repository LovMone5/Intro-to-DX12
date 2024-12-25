#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <iostream>

using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;

ostream& XM_CALLCONV operator<<(ostream& out, FXMVECTOR& v) {
	XMFLOAT3 t;
	XMStoreFloat3(&t, v);
	out << "(" << t.x << ", " << t.y << ", " << t.z << ")";

	return out;
}

int main() {
	if (!XMVerifyCPUSupport()) {
		cout << "DirectX math not supported!" << endl;
		return 0;
	}
	const auto& p = XMVectorZero();
	const auto& q = XMVectorSplatOne();
	const auto& u = XMVectorSet(1.0f, 2.0f, 3.0f, 0.0f);
	const auto& v = XMVectorReplicate(-2.0f);
	const auto& w = XMVectorSplatZ(u);

	cout << "p = " << p << endl;
	cout << "q = " << q << endl;
	cout << "u = " << u << endl;
	cout << "v = " << v << endl;
	cout << "w = " << w << endl;

	cout << "u + v = " << u + v << endl;
	cout << "10.f * u = " << 10.f * u << endl;

	XMVECTOR proj, perp;
	auto n = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	XMVector3ComponentsFromNormal(&proj, &perp, v, u);
	cout << "proj = " << proj << endl;
	cout << "perp = " << perp << endl;
	

	cout << "||proj|| = " << XMVector3Length(proj) << endl;
	cout << "Estimate ||proj|| = " << XMVector3LengthEst(proj) << endl;
	proj = XMVector3Normalize(proj);
	cout << "proj / ||proj|| = " << proj << endl;


	return 0;
}