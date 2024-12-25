#include <iostream>
#include <vector>
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

vector<vector<int>> transpose(const vector<vector<int>>& m);
int determinant(const vector<vector<int>>& m);
vector<vector<int>> minor(const vector<vector<int>>& m, const int r, const int c);
vector<vector<double>> inverse(const int det, const vector<vector<int>>& m);
int cofactor(const vector<vector<int>>& m, const int r, const int c);

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
	const auto& C = A * B;

	const auto& D = XMMatrixTranspose(A);

	auto det = XMMatrixDeterminant(A);
	const auto& E = XMMatrixInverse(&det, A);

	const auto& F = A * E;

	cout << "A = " << endl << A << endl;
	cout << "B = " << endl << B << endl;
	cout << "C = A * B" << endl << C << endl;
	cout << "D = transpose(A)" << endl << D << endl;
	cout << "det = determinant(A)" << endl << det << endl;
	cout << "E = inverse(A)" << endl << E << endl;
	cout << "F = A * E" << endl << F << endl;

	vector<vector<int>> matrix1(3, vector<int>(4));
	matrix1[0][0] = 1,	matrix1[0][1] = 2,	matrix1[0][2] = 3,	matrix1[0][3] = 4;
	matrix1[1][0] = 5,	matrix1[1][1] = 6,	matrix1[1][2] = 7,	matrix1[1][3] = 8;
	matrix1[2][0] = 9,	matrix1[2][1] = 10,	matrix1[2][2] = 11,	matrix1[2][3] = 12;

	const auto& res1 = transpose(matrix1);
	cout << "my transpose of m" << endl;
	for (int i = 0; i < res1.size(); i++) {
		for (int j = 0; j < res1[0].size(); j++)
			cout << res1[i][j] << '\t';
		cout << endl;
	}

	vector<vector<int>> matrix2(4, vector<int>(4));
	matrix2[0][0] = 1, matrix2[0][1] = 3, matrix2[0][2] = 5, matrix2[0][3] = 9;
	matrix2[1][0] = 1, matrix2[1][1] = 3, matrix2[1][2] = 1, matrix2[1][3] = 7;
	matrix2[2][0] = 4, matrix2[2][1] = 3, matrix2[2][2] = 9, matrix2[2][3] = 7;
	matrix2[3][0] = 5, matrix2[3][1] = 2, matrix2[3][2] = 0, matrix2[3][3] = 9;

	int det1 = determinant(matrix2);
	cout << "my det of m" << endl;
	cout << det1 << endl;

	const auto& inm = inverse(det1, matrix2);
	cout << "my inverse of m" << endl;
	for (int i = 0; i < inm.size(); i++) {
		for (int j = 0; j < inm[0].size(); j++)
			cout << inm[i][j] << '\t';
		cout << endl;
	}

	return 0;
}

vector<vector<int>> transpose(const vector<vector<int>>& m) {
	if (m.size() == 0) return vector<vector<int>>();

	vector<vector<int>> res(m[0].size(), vector<int>(m.size()));
	for (int i = 0; i < m.size(); i++)
		for (int j = 0; j < m[0].size(); j++)
			res[j][i] = m[i][j];

	return res;
}

int determinant(const vector<vector<int>>& m) {
	if (m.size() == 1)
		return m[0][0];

	int res = 0;
	for (int i = 0; i < m.size(); i++) {
		if (i % 2)
			res -= m[0][i] * determinant(minor(m, 0, i));
		else
			res += m[0][i] * determinant(minor(m, 0, i));
	}

	return res;
}

vector<vector<int>> minor(const vector<vector<int>>& m, const int r, const int c) {
	int n = m.size() - 1;
	vector<vector<int>> res(n, vector<int>(n));
	int x = 0;
	for (int i = 0; i <= n; i++) {
		if (i == r)
			continue;
		int y = 0;
		for (int j = 0; j <= n; j++) {
			if (j != c)
				res[x][y++] = m[i][j];
		}
		x++;
	}

	return res;
}

int cofactor(const vector<vector<int>>& m, const int r, const int c) {
	return pow(-1, r + c) * determinant(minor(m, r, c));
}

vector<vector<double>> inverse(const int det, const vector<vector<int>>& m) {
	int n = m.size();
	vector<vector<int>> t(n, vector<int>(n));
	vector<vector<double>> res(n, vector<double>(n));

	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			t[i][j] = cofactor(m, i, j);
		}
	}

	t = transpose(t);
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			res[i][j] = (double)t[i][j] / det;
		}
	}

	return res;
}
