//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

package lte.corenetwork.nodes.cars;

simple VehicleRerouteApp
{
    parameters:
        // Edge kontrol döngüsü periyodu
        double checkInterval @unit(s) = default(1s);

        // Hedef edge ID (newRoute / changeTarget / changeVehicleRoute için)
        string targetEdgeId = default("5170334#0");

        // Bu edge'e gelince rota değiştir
        string watchEdgeId = default("826449813");

        // changeRoute() için yapay seyahat süresi
        // 3600s → araç o edge'den kaçınır
        double artificialTravelTime @unit(s) = default(3600s);

        @class(VehicleRerouteApp);
        @display("i=block/app");
}
