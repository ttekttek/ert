#  Copyright (C) 2015  Statoil ASA, Norway. 
#   
#  The file 'test_ecl_init_file.py' is part of ERT - Ensemble based Reservoir Tool.
#   
#  ERT is free software: you can redistribute it and/or modify 
#  it under the terms of the GNU General Public License as published by 
#  the Free Software Foundation, either version 3 of the License, or 
#  (at your option) any later version. 
#   
#  ERT is distributed in the hope that it will be useful, but WITHOUT ANY 
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or 
#  FITNESS FOR A PARTICULAR PURPOSE.   
#   
#  See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html> 
#  for more details.


from ert.test import ExtendedTestCase
from ert.ecl import Ecl3DKW , EclKW, EclTypeEnum, EclRestartFile , EclFile, FortIO, EclFileFlagEnum , EclGrid

class RestartFileTest(ExtendedTestCase):
    def setUp(self):
        self.grid_file = self.createTestPath("Statoil/ECLIPSE/Gurbat/ECLIPSE.EGRID")
        self.rst_file  = self.createTestPath("Statoil/ECLIPSE/Gurbat/ECLIPSE.UNRST")
        

    def test_load(self):
        g = EclGrid( self.grid_file )
        f = EclRestartFile( g , self.rst_file )

        head = f["INTEHEAD"][0]
        self.assertTrue( isinstance( head , EclKW ))
        
        swat = f["SWAT"][0]
        self.assertTrue( isinstance( swat , Ecl3DKW ))
        
        pressure = f["PRESSURE"][0]
        self.assertTrue( isinstance( pressure , Ecl3DKW ))
        
        

