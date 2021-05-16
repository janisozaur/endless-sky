#include "es-test.hpp"

// Include only the tested class's header.
#include "../../source/ImageBuffer.h"

namespace { // test namespace
// #region mock data
// #endregion mock data



// #region unit tests
SCENARIO( "JPEG images can be loaded", "[ReadJPG]") {
	GIVEN( "A jpeg file and ImageBuffer" ) {
		ImageBuffer im;
		auto path = "images/land/badlands0.jpg";
		WHEN( "the image is loaded" ) {
			im.Read(path, 0);
			
			THEN( "its first and last pixels are present" ) {
				CHECK( im.Width() == 7 );
				CHECK( im.Height() == 10 );
				CHECK( im.Pixels()[0] == 12 );
				CHECK( im.Pixels()[im.Width() * im.Height() - 1] == 13 );
			}
		}
	}
}
// #endregion unit tests

} // test namespace
