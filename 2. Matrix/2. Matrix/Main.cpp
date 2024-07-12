#include <iostream>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;

ostream& XM_CALLCONV operator<<(ostream& out, FXMVECTOR v) {
	XMFLOAT4 dest;
	XMStoreFloat4(&dest, v);

	out << "(" << dest.x << ", " << dest.y << ", " << dest.z << ", " << dest.w << ")" << endl;

	return out;
}

ostream& XM_CALLCONV operator<<(ostream& out, FXMMATRIX m) {
	for (int i = 0; i < 4; i++) {
		out << XMVectorGetX(m.r[i]) << '\t';
		out << XMVectorGetY(m.r[i]) << '\t';
		out << XMVectorGetZ(m.r[i]) << '\t';
		out << XMVectorGetW(m.r[i]);
		out << endl;
	}

	return out;
}

int main() {
	if (!XMVerifyCPUSupport()) {
		cout << "directx math not supported" << endl;
		return 0;
	}

	XMMATRIX A(	1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 2.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 4.0f, 0.0f,
				1.0f, 2.0f, 3.0f, 1.0f);
	XMMATRIX B = XMMatrixIdentity();
	auto C = A * B;

	auto D = XMMatrixTranspose(A);

	auto det = XMMatrixDeterminant(A);
	auto E = XMMatrixInverse(&det, A);

	auto F = A * E;

	cout << "A = " << endl << A << endl;
	cout << "B = " << endl << B << endl;
	cout << "C = A * B" << endl << C << endl;
	cout << "D = transpose(A)" << endl << D << endl;
	cout << "det = determinant(A)" << endl << det << endl;
	cout << "E = inverse(A)" << endl << E << endl;
	cout << "F = A * E" << endl << F << endl;

	return 0;
}
