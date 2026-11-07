#pragma once
#include <Assets/IAsset.h>
#include <Diligent/Graphics/GraphicsEngine/interface/Texture.h>
#include <Diligent/Common/interface/RefCntAutoPtr.hpp>
#include <Math/Math.h>
#include <unordered_map>

namespace Nuclear
{
	namespace Assets
	{
		/// Synchronous R8 SDF upload; Data is borrowed only for Create().
		struct FontCreationDesc
		{
			Uint32 AtlasWidth = 0; ///< Atlas width in pixels.
			Uint32 AtlasHeight = 0; ///< Atlas height in pixels.
			const void* Data = nullptr; ///< Tightly packed R8 texels, bottom row first.
		};
		/// Metrics for one glyph; plane bounds and advances are normalized to font em units.
		struct FontCharacter {
			Uint32 CodePoint = 0; ///< Unicode scalar represented by this glyph.
			Math::ivec2 Size = {}; ///< Atlas box size in pixels, retained for existing callers.
			Math::ivec2 Bearing = {}; ///< Pixel offset from baseline, retained for existing callers.
			float Advance = 0; ///< Fractional horizontal advance in em units.
			Math::Vector4 PlaneBounds = {}; ///< Left, bottom, right, top relative to baseline, in em units.
			Math::Vector4 AtlasBounds = {}; ///< Left, top, right, bottom UVs for the bottom-up atlas.
		};

		/// Library-owned font atlas and layout metrics. Widgets borrow Font pointers.
		class NEAPI Font : public IAsset
		{
		public:
			Font();
			~Font();


			/// Uploads on the graphics thread. Invalid input or GPU failure leaves State::Unknown.
			void Create(const FontCreationDesc& desc);
			/// Looks up a glyph, falling back to '?'; null when neither is present.
			const FontCharacter* FindCharacter(Uint32 codepoint) const;
			/// Returns the normalized pair kerning adjustment, or zero when absent.
			float GetKerning(Uint32 previous, Uint32 current) const;


		//private:
			std::unordered_map<Uint32, FontCharacter> Characters; ///< Codepoint-to-metric map.
			std::unordered_map<Uint64, float> Kerning; ///< Pair key: previous << 32 | current; em-unit values.
			float Ascender = .8f; ///< Distance from line top to baseline in em units.
			float LineHeight = 1.2f; ///< Baseline spacing in em units.

			Diligent::RefCntAutoPtr<Diligent::ITextureView> mTextureView; ///< Owned GPU atlas reference.
		};


	}
}
