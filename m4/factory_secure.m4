AC_DEFUN([FACTORY_SECURE_BINARY],[
	FACTORY_CXX_FLAG([-pie])
	FACTORY_CXX_FLAG([-fpie])
	FACTORY_CXX_FLAG([-fPIE])
])
