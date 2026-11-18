#pragma once
#include "MyMath.h"
#include "Logger.h"
#include "externals/json.hpp"
#include <cmath>

/// <summary>
/// エディターの配置データ(レール・的・敵)を JSON に読み書きするときの、数値の安全な変換
/// NaN(数ではない値)や無限大をそのまま保存すると JSON には null として書き出され、
/// 次に起動したときに数値として読めずに止まってしまうため、保存するときも読み込むときも決まった値に置き換える
/// </summary>
namespace EditorJson{
	// 置き換えるときの既定の値(座標・向きは原点・0 にする)
	constexpr float kDefaultValue = 0.0f;
	constexpr Vector3 kDefaultVector3 = {kDefaultValue, kDefaultValue, kDefaultValue};
	// 3つの値を持つ配列の要素数
	constexpr size_t kVector3ElementCount = 3;

	/// <summary>
	/// 有限な値ならそのまま、NaN・無限大なら fallback を返す(保存する前の値の確認に使う)
	/// </summary>
	inline float ToFinite(float value,float fallback = kDefaultValue){
		if(std::isfinite(value)){
			return value;
		}
		Logger::Log("EditorJson: NaN または無限大の値を保存しようとしたため、既定の値に置き換えました\n");
		return fallback;
	}

	/// <summary>
	/// Vector3 を JSON の配列 [x, y, z] にする(NaN・無限大の要素は fallback の同じ要素に置き換える)
	/// </summary>
	inline nlohmann::json WriteVector3(const Vector3& value,const Vector3& fallback = kDefaultVector3){
		return {ToFinite(value.x,fallback.x), ToFinite(value.y,fallback.y), ToFinite(value.z,fallback.z)};
	}

	/// <summary>
	/// JSON の値を float として読む(数値でない(null など)・NaN・無限大のときは fallback を返す)
	/// </summary>
	inline float ReadFloat(const nlohmann::json& json,float fallback = kDefaultValue){
		if(!json.is_number()){
			Logger::Log("EditorJson: 数値でない値(null など)を読み込んだため、既定の値に置き換えました\n");
			return fallback;
		}
		const float value = json.get<float>();
		if(!std::isfinite(value)){
			Logger::Log("EditorJson: NaN または無限大の値を読み込んだため、既定の値に置き換えました\n");
			return fallback;
		}
		return value;
	}

	/// <summary>
	/// JSON の配列 [x, y, z] を Vector3 として読む
	/// 配列でない・要素が足りないときは fallback を、要素ごとに数値でないものは fallback の同じ要素を使う
	/// </summary>
	inline Vector3 ReadVector3(const nlohmann::json& json,const Vector3& fallback = kDefaultVector3){
		if(!json.is_array() || json.size() < kVector3ElementCount){
			Logger::Log("EditorJson: 3つの数値の配列でない値を読み込んだため、既定の値に置き換えました\n");
			return fallback;
		}
		return {ReadFloat(json[0],fallback.x), ReadFloat(json[1],fallback.y), ReadFloat(json[2],fallback.z)};
	}
}
